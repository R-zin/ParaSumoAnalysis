# Benchmark results (auto-generated, measured)

Environment:
- Date (UTC): 2026-09-06 17:53:26
- Machine: arm64 / arm
- System: Darwin 25.5.0
- Python: 3.14.6
- CPU: Apple M2, 8 logical cores, 16 GiB RAM

Timing = pure computation (CSV I/O excluded), median of 7 repetitions of best-of-N inner runs.

Two baselines are reported: **S(serial)** uses the dedicated serial program as T(1); **S(omp1)** uses the OpenMP kernel restricted to one thread, isolating pure scaling from the fixed cost of entering an OpenMP parallel region. Efficiency uses the serial baseline.

| Dataset | Serial (s) | Threads | Time (s) | S(serial) | S(omp1) | Efficiency |
|---------|-----------|---------|----------|-----------|---------|------------|
| 10K | 0.000024 | 1 | 0.000030 | 0.79 | 1.00 | 79.4% |
| 10K | 0.000024 | 2 | 0.000027 | 0.87 | 1.10 | 43.5% |
| 10K | 0.000024 | 4 | 0.000026 | 0.90 | 1.13 | 22.5% |
| 10K | 0.000024 | 8 | 0.000051 | 0.46 | 0.58 | 5.8% |
| 100K | 0.000239 | 1 | 0.000299 | 0.80 | 1.00 | 80.1% |
| 100K | 0.000239 | 2 | 0.000165 | 1.45 | 1.81 | 72.4% |
| 100K | 0.000239 | 4 | 0.000098 | 2.43 | 3.03 | 60.7% |
| 100K | 0.000239 | 8 | 0.000153 | 1.56 | 1.95 | 19.5% |
| 1M | 0.002400 | 1 | 0.003000 | 0.80 | 1.00 | 80.0% |
| 1M | 0.002400 | 2 | 0.001561 | 1.54 | 1.92 | 76.9% |
| 1M | 0.002400 | 4 | 0.000891 | 2.69 | 3.37 | 67.3% |
| 1M | 0.002400 | 8 | 0.000937 | 2.56 | 3.20 | 32.0% |
| 5M | 0.012096 | 1 | 0.015110 | 0.80 | 1.00 | 80.1% |
| 5M | 0.012096 | 2 | 0.007771 | 1.56 | 1.94 | 77.8% |
| 5M | 0.012096 | 4 | 0.004333 | 2.79 | 3.49 | 69.8% |
| 5M | 0.012096 | 8 | 0.004426 | 2.73 | 3.41 | 34.2% |
| 10M | 0.024326 | 1 | 0.030218 | 0.81 | 1.00 | 80.5% |
| 10M | 0.024326 | 2 | 0.015516 | 1.57 | 1.95 | 78.4% |
| 10M | 0.024326 | 4 | 0.008642 | 2.82 | 3.50 | 70.4% |
| 10M | 0.024326 | 8 | 0.008834 | 2.75 | 3.42 | 34.4% |
