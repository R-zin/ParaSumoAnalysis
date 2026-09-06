# Verification methodology

The central correctness requirement is

> serial result == OpenMP result  (for every test dataset)

This document explains exactly how "==" is defined and why.

## Why bitwise equality is the wrong test

The OpenMP kernel uses a reduction: the N additions are split into `p`
partial sums that are combined at the end. Floating-point addition is **not
associative**, so changing the grouping changes the result in the last few
bits, e.g.

```
serial : (((a+b)+c)+d)+...
2-way  : (a+c+...) + (b+d+...)
```

For a dataset whose totals are ~1e9, a perfectly correct parallel reduction
differs from the serial sum by roughly machine-epsilon × magnitude ≈
2.2e-16 × 1e9 ≈ 2e-7 in absolute terms, and by more once many roundings
accumulate. So:

* demanding `serial == openmp` bitwise **fails on correct code**, and
* demanding `|serial - openmp| < 1e-6` absolute **also fails on correct code**
  once totals exceed ~1e6, because the legitimate reordering error already
  exceeds 1e-6.

## The chosen test: magnitude-scaled relative tolerance

Two aggregates `a` (serial) and `b` (OpenMP) are declared equal when

```
|a - b|  <=  kResultTolerance * max(1, |a|, |b|)
        with  kResultTolerance = 1e-9
```

The `max(1, …)` floor keeps the test meaningful for values near zero.

`1e-9` was chosen deliberately:

* the **observed** reordering noise on the largest dataset (10M rows, totals
  ~1e10) is ~1e-13 relative — about 4 orders of magnitude *below* the
  tolerance, so correct code passes with large margin;
* any **real defect** — a data race, a missed record, a logic error — moves a
  total by far more than 1e-9 relative, so it cannot slip through.

`vehicle_count` and the speed histogram are compared **exactly** (they are
integers; any difference is a genuine bug).

## Where the test runs

* `tests/test_correctness.cpp` — asserts the hand-computed 3-vehicle
  specification example, then asserts serial==OpenMP (relative tolerance) on
  a 100,000-record deterministic dataset for team sizes 1, 2, 4, 8.
* `traffic_analysis_openmp --verify` — recomputes the serial reference at
  run time and prints `Verification vs serial ... PASS/FAIL`.
* `benchmarks/run_benchmark.py` — every benchmarked cell uses the same
  verified binaries.

Run the test suite with:

```
ctest --test-dir build --output-on-failure
# or directly:
./build/test_correctness
```
