# Module II — OpenMP Parallelization

## 1. Identifying the parallelizable block

The candidate loop is the aggregation over the vehicle array
(`src/common/traffic_analysis.cpp`, mirrored in
`src/openmp/traffic_analysis_openmp_kernel.cpp`):

```cpp
for (std::size_t i = 0; i < vehicles.size(); ++i) {
    total_speed        += vehicles[i].speed;
    total_travel_time  += vehicles[i].travel_time;
    total_waiting_time += vehicles[i].waiting_time;
    total_distance     += vehicles[i].distance;
    // + max tracking, delay ratio, histogram bucket
}
```

### Why each vehicle can be processed independently

* **Loop-carried independence of reads.** Iteration `i` reads only
  `vehicles[i]`; no iteration ever writes to the array or to any other
  vehicle's record. The input is effectively `const`.
* **Associative/commutative aggregation.** The loop's only cross-iteration
  coupling is through *reduction* variables (sums, maxima) and the histogram.
  Sums and maxima are associative and commutative, so the final result does
  not depend on the order in which records are combined — exactly the
  property OpenMP `reduction` exploits.
* **Uniform per-iteration cost.** Every iteration does the same handful of
  FLOPs, so a static partition gives a balanced workload with no dynamic
  scheduling overhead.

This is the *embarrassingly parallel* reduction pattern: the loop can be
split into `p` contiguous chunks, each aggregated independently, and the
partial results combined at the end.

## 2. Avoiding race conditions

A naive shared `total += v.speed` inside a parallel loop is a **data race**:
two threads can read-modify-write the same variable concurrently and lose
updates. We eliminate every race *by construction*:

| Shared quantity        | Mechanism                                   | Why it's safe |
|------------------------|---------------------------------------------|---------------|
| sum aggregates (5)     | `#pragma omp ... reduction(+ : ...)`        | each thread accumulates a private copy; OpenMP combines them once |
| maxima (2)             | `reduction(max : ...)`                       | same, with max as the combine operator |
| speed histogram        | per-thread private array + 7 `omp atomic` adds | not a scalar reduction, so merged explicitly |
| `vehicles[]`           | read-only inside the region                  | no writes ⇒ no race |
| loop-local temporaries | declared inside the loop body                | private to each iteration |

The `default(none)` clause on the parallel region forces every shared
variable to be listed explicitly, so an accidental capture is a *compile
error*, not a latent race.

## 3. The OpenMP construct used

```cpp
#pragma omp parallel if (num_threads > 0) num_threads(num_threads) \
    default(none)                                                  \
    shared(vehicles, n, result, team_size_report)                  \
    firstprivate(num_threads)                                      \
    reduction(+ : total_speed, total_travel_time, total_waiting_time, \
                  total_distance, total_delay_ratio)               \
    reduction(max : max_travel_time, max_waiting_time)
{
    std::array<std::uint64_t, 7> local_histogram{};   // private per thread

    #pragma omp single
    team_size_report = omp_get_num_threads();          // record real team size

    #pragma omp for schedule(static)
    for (std::size_t i = 0; i < n; ++i) {
        // accumulate into private reduction copies + local_histogram
    }                                                   // implicit barrier

    for (std::size_t b = 0; b < 7; ++b)                 // merge partial histograms
        if (local_histogram[b])
            #pragma omp atomic
            result.speed_histogram[b] += local_histogram[b];
}
```

* `reduction(+:…)` / `reduction(max:…)` — thread-private accumulators,
  combined by the runtime.
* `schedule(static)` — contiguous equal chunks; correct because iterations
  are uniform.
* `#pragma omp single` — exactly one thread records the team size.
* `#pragma omp atomic` — cheap merge of the 7-bucket histograms
  (7 atomics × p threads, negligible vs N iterations).
* `if (num_threads > 0)` — lets callers pass 0 to defer to the runtime
  default without an illegal `num_threads(0)`.

## 4. Threads, work distribution, synchronization

* **Threads.** The team size is `num_threads` (or the runtime default,
  typically the number of logical cores). The program prints
  `omp_get_num_procs()` so it never pretends to have more hardware than
  exists.
* **Work distribution.** `schedule(static)` assigns iteration ranges
  `[k·N/p, (k+1)·N/p)` to thread `k`. Because each iteration costs the same,
  this is balanced and has zero scheduling overhead.
* **Reduction.** Each thread's private partial sums live in registers/cache;
  the combine happens once, in a tree of depth O(log p), at region end.
* **Synchronization overhead.** There are only two synchronization points:
  the barrier at the end of `omp for` and the reduction/histogram merge at
  region end. Both are O(p), independent of N. For large N this overhead is
  amortized to nothing; for small N it can exceed the saved work, which is
  precisely what the benchmarks show.

## 5. Floating-point determinism caveat

Parallel reduction reorders floating-point additions, and FP addition is not
associative. The OpenMP result can therefore differ from the serial result by
a few ulps per aggregate — for a 1M-row dataset with totals ~1e9 this is an
absolute difference of ~1e-5, i.e. a **relative** difference of ~1e-13.

This is expected and is not a bug. Verification therefore uses an explicit
**magnitude-scaled relative tolerance** (`kResultTolerance = 1e-9`,
`results_equal()`), not bitwise equality. 1e-9 is ~4 orders of magnitude
looser than the observed reordering noise yet far tighter than any real
logic error or race, so it catches genuine bugs while tolerating legitimate
reordering. See `tests/test_correctness.cpp` and the `--verify` flag.

## 6. Why speedup is not linear (measured, not assumed)

The benchmark results in `benchmarks/results/` show sub-linear scaling. The
causes, in order of importance for *this* kernel:

1. **Memory-bandwidth bound.** Each record needs ~3 FLOPs but ~32 bytes of
   reads, an arithmetic intensity of ~0.1 FLOP/byte. A few cores already
   saturate the memory bus; additional threads queue on bandwidth and add
   nothing.
2. **Limited parallel fraction (Amdahl).** Loading, averaging, and printing
   are serial. Only the aggregation loop is parallel, so the maximum speedup
   is capped regardless of thread count.
3. **Reduction & merge overhead.** Combining partial results costs O(p) work
   at the end; on tiny datasets it can exceed the parallel gain.
4. **Thread-team startup.** Creating/waking the team costs microseconds —
   comparable to the *entire* 10K-vehicle computation, which is why small
   datasets show speedup ≈ 1 or even < 1.
5. **Cache behaviour.** Each thread streams a disjoint chunk; with 8 threads
   the working set exceeds cache and every read goes to main memory.
6. **Core count.** The machine has 8 logical cores, so thread counts beyond
   that cannot help and may hurt.

These are the honest, measured reasons the project reports the numbers it
does rather than claiming near-linear speedup.
