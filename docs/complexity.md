# Module I — Time and Space Complexity

## Time complexity: O(N)

Both the serial and the OpenMP kernels make **exactly one pass** over the N
vehicle records. The work done per record is a fixed, constant set of
operations independent of N:

* 5 floating-point additions (speed, travel, waiting, distance, delay ratio),
* 2 `max` comparisons,
* 1 division (guarded),
* a constant number of comparisons to pick a histogram bucket, and one
  increment.

There is no nested loop, no per-record dependence on other records, and no
data structure whose access cost grows with N. Therefore

```
T(N) = c · N   ⇒   T(N) ∈ O(N)
```

for some small constant `c`. Doubling the dataset exactly doubles the number
of loop iterations, which is confirmed empirically: the measured computation
time grows linearly with dataset size (see `benchmarks/graphs/time_vs_dataset_size.svg`).

### The OpenMP version is still O(N)

Parallelization changes the *constant factor*, not the asymptotic class. With
`p` threads the idealized wall-clock time is

```
T_p(N) ≈ c · N / p  +  parallel-overhead(p)
```

which is still Θ(N) for fixed `p`. OpenMP adds a term that is independent of
N (thread-team startup, reduction tree of depth O(log p), the 7-atomic
histogram merge of cost O(p)). That overhead is exactly why small datasets
sometimes show *no* speedup: when `c·N/p` is already microseconds, the
constant parallel overhead dominates.

## Space complexity: O(N)

The analyzer stores the vehicle records in memory:

```
std::vector<Vehicle>   // N × sizeof(Vehicle)
```

`Vehicle` holds a `std::string id` plus four `double`s, so memory grows
linearly with N ⇒ **O(N)**. This is a deliberate design choice: keeping the
records resident lets us (a) run repeated benchmark iterations over the same
in-memory data and (b) exclude file I/O from the timed region.

The *additional* space used by the computation itself is O(1) for the serial
version and **O(p)** for the OpenMP version (one private histogram and one
set of reduction copies per thread), both negligible next to the dataset.

> If memory were a constraint, the kernel could be made streaming/O(1) extra
> space by aggregating while reading. We deliberately load first so that the
> benchmark measures computation, not disk.

## What is benchmarked (and what is not)

The requirement is to measure **computational** performance, not disk
performance. Both programs therefore separate the phases:

```
load CSV  ──►  [ start timer ──► analyze() ──► stop timer ]  ──► print
   │                        ▲                                       │
 excluded            the only measured region                 excluded
```

Timing uses `omp_get_wtime()`, and the reported figure is
the **best of `--iterations N`** repetitions inside the binary, further
smoothed by the benchmark harness taking the **median of 7 outer
repetitions**. File I/O, argument parsing, and printing are never inside the
timed region.
