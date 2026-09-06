# SumoPara — Parallel Analysis of SUMO Traffic Data Using OpenMP

An academic HPC project: analyze per-vehicle traffic data produced by
**SUMO** (Simulation of Urban MObility), first with a **serial C++17**
program and then with an **OpenMP parallel** version, followed by honest
performance benchmarking (speedup & efficiency vs dataset size and thread
count).

> **Scope.** SUMO is used *only to generate data*. SUMO's internal simulation
> engine is **not** modified or parallelized. The parallelization target is
> the vehicle-data analysis loop over the exported CSV.

```
SUMO Simulation ──► Vehicle Output (XML) ──► CSV ──► C++ Analysis
                                                     ├── Serial   (traffic_analysis_serial)
                                                     └── OpenMP   (traffic_analysis_openmp)
                                                                │
                                                                ▼
                                                     Performance Analysis
                                                     (benchmarks/run_benchmark.py)
```

## Features

* C++17, clean separation of **input / computation / output / benchmarking**.
* Serial and OpenMP kernels kept logically equivalent and **verified equal**
  (magnitude-scaled relative tolerance, see `docs/verification.md`).
* Metrics: vehicle count, total/average speed, travel time, waiting time,
  distance, max travel/waiting time, average delay ratio, 7-bucket speed
  histogram, congestion classification.
* Deterministic synthetic dataset generator (works **without SUMO**).
* Real SUMO demo scenario + converter (`sumo/`, `tools/`).
* Reproducible benchmark harness producing measured tables + SVG graphs.
* No third-party C++ or Python dependencies (benchmark & plots are stdlib-only).

## Build

CMake is canonical (CLion project). On macOS the default Apple clang has no
OpenMP, so point CMake at Homebrew LLVM (`brew install llvm`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build -j
```

On Linux the default compiler usually has OpenMP already:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

A convenience Makefile forwards the common tasks (`make`, `make test`,
`make datasets`, `make benchmark`); pass the compiler on macOS with
`make CXX=/opt/homebrew/opt/llvm/bin/clang++`.

Targets: `traffic_analysis_serial`, `traffic_analysis_openmp`,
`generate_dataset`, `test_correctness`.

## Quick start

```bash
# 1. generate deterministic datasets (10K / 100K / 1M / 5M / 10M)
./build/generate_dataset 1000000 data/large.csv        # or: make datasets

# 2. run the tests (serial == OpenMP, hand-verified example, malformed input)
ctest --test-dir build --output-on-failure

# 3. analyze one dataset, serial and parallel (must agree)
./build/traffic_analysis_serial data/large.csv
./build/traffic_analysis_openmp data/large.csv --threads 8 --verify

# 4. full benchmark: tables + graphs from *measured* runs
python3 benchmarks/run_benchmark.py                    # or: make benchmark
```

## Usage

```
traffic_analysis_serial <input.csv> [--bench] [--iterations N]
traffic_analysis_openmp <input.csv> [--threads N] [--bench] [--iterations N] [--verify]
generate_dataset <vehicle_count> [output.csv]
```

`--bench` prints a machine-readable `BENCH` timing line (pure computation,
CSV I/O excluded). `--verify` makes the OpenMP program recompute the serial
reference and print PASS/FAIL. `--threads 0` lets the OpenMP runtime choose.

## Project layout

```
├── CMakeLists.txt          build (OpenMP optional, LLVM fallback on macOS)
├── Makefile                convenience wrapper
├── include/
│   └── traffic_analysis.hpp            data model + kernel declarations
├── src/
│   ├── common/traffic_analysis.cpp     CSV loader, serial kernel, compare
│   ├── serial/traffic_analysis_serial.cpp
│   └── openmp/traffic_analysis_openmp.cpp
│       └── openmp/traffic_analysis_openmp_kernel.cpp
├── tools/
│   ├── generate_dataset.cpp            deterministic synthetic datasets
│   └── sumo_tripinfo_to_csv.py         SUMO tripinfo XML -> CSV
├── sumo/                               real demo scenario (see sumo/README.md)
├── data/                               datasets (generated; sumo_tripdata.csv committed)
├── tests/test_correctness.cpp
├── benchmarks/
│   ├── run_benchmark.py                measured benchmark harness
│   ├── plot_results.py                 stdlib-only SVG graphs
│   ├── results/                        measured CSV + Markdown tables
│   └── graphs/                         SVG figures
└── docs/                               Module I & II write-ups
```

## Documentation

| Doc | Covers |
|-----|--------|
| `docs/objective.md`        | Module I — objective, workflow, where SUMO fits, environment |
| `docs/algorithm.md`        | Module I — serial & parallel pseudocode, flowchart, worked example |
| `docs/complexity.md`       | Module I — O(N) time/space analysis, what is benchmarked |
| `docs/parallelization.md`  | Module II — parallelizable block, race avoidance, constructs, scaling limits |
| `docs/verification.md`     | how "serial == OpenMP" is defined and tested |
| `docs/testing.md`          | the three test layers |
| `docs/sumo_integration.md` | scenario provenance & validation |
| `docs/results.md`          | measured benchmark tables + discussion |

## Reproducibility

* All performance numbers come from **actually running** the binaries;
  nothing is fabricated.
* Timing excludes file I/O and uses `std::chrono::high_resolution_clock`;
  the harness reports the median of repeated runs.
* Same input data, compiler, optimization level, machine, and metric
  calculations are used for serial and parallel experiments.
* Hardware/software environment is recorded in `docs/objective.md` and in
  `benchmarks/results/benchmark_summary.md`.

## License / attribution

SUMO is © German Aerospace Center (DLR) and contributors, EPL-2.0. This
repository only *consumes* SUMO output.
