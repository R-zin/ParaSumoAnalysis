# Testing

Three deterministic layers, all runnable without SUMO.

## 1. Correctness tests (`tests/test_correctness.cpp`)

```
./build/test_correctness        # or: ctest --test-dir build --output-on-failure
```

* **Test 1 — specification example.** The 3-vehicle dataset from the project
  brief is checked against hand-computed values (avg_speed = 15,
  total_travel = 270, avg_distance = 1033.33…, …). The OpenMP kernel must
  reproduce the same values for 1/2/4/8 threads.
* **Test 2 — serial ≡ OpenMP.** A 100,000-record deterministic dataset
  (fixed-seed LCG, no external files) is analyzed by both kernels; the
  results must agree within the relative tolerance `1e-9` for every
  supported team size. Thread counts above the available hardware are
  detected and reported as SKIP rather than silently run.
* **Test 3 — malformed input.** A CSV containing non-numeric, short,
  over-long, and negative rows must load the 2 valid rows, skip and count the
  4 bad ones, and a file with zero valid rows must fail cleanly (exit 1), not
  crash.

## 2. Run-time verification (`--verify`)

```
./build/traffic_analysis_openmp data/large.csv --threads 8 --verify
```

Recomputes the serial reference and prints
`Verification vs serial (relative tolerance 1e-09): PASS/FAIL`. Exit code 2
on mismatch, so it can gate scripts.

## 3. Benchmark sanity

The benchmark harness uses the verified binaries, and reports median ±
min/max over repetitions so outlier runs are visible rather than hidden.

## What is *not* covered (and why)

* **The CSV loader's locale handling** is intentionally strict
  (`std::from_chars` is locale-independent); there is no fuzzing harness,
  but malformed-input behaviour is covered by Test 3.
* **SUMO itself** is third-party and out of scope; only the converter's
  output is validated against the analyzers.
