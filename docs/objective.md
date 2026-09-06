# Module I — Objective and Description

## Title

**Parallel Analysis of SUMO Traffic Simulation Data Using OpenMP**

## Objective

Traffic micro-simulators such as SUMO (Simulation of Urban MObility) produce
very large volumes of per-vehicle output: for every simulated vehicle they
record its speed, travel time, waiting time, and distance travelled.
Extracting useful traffic-engineering knowledge from this raw output —
average speeds, total delay, congestion indicators, speed distributions —
means aggregating over *millions* of independent vehicle records.

The objective of this project is to:

1. implement a correct, validated **serial** C++17 program that computes
   these aggregate traffic metrics from a CSV dataset, and
2. implement an **OpenMP parallel** version of the *same* computation,
3. **measure** the performance difference honestly (speedup and efficiency
   across dataset sizes and thread counts), and
4. analyse *why* the observed scaling is what it is.

## What the system does

```
SUMO Simulation ──► Vehicle/Traffic Output (XML)
                          │   tools/sumo_tripinfo_to_csv.py
                          ▼
                    CSV Dataset ──► C++ Analysis
                                    ├── Serial Version      (traffic_analysis_serial)
                                    └── OpenMP Version      (traffic_analysis_openmp)
                                              │
                                              ▼
                                    Performance Analysis
                                    (benchmarks/run_benchmark.py)
```

Given a CSV of vehicle records

```
vehicle_id,speed,travel_time,waiting_time,distance
veh_0,7.1380,1012.2263,189.5794,7044.8040
...
```

the analyzer computes, in a single pass over the data:

| Category   | Metrics |
|------------|---------|
| Count      | vehicle count |
| Speed      | total & average speed, 7-bucket speed histogram |
| Travel time| total, average, maximum |
| Waiting    | total, average, maximum waiting time |
| Distance   | total & average distance |
| Delay      | average delay ratio (waiting / travel) |
| Congestion | classification from average speed (FREE-FLOW / SLOW / CONGESTED) |

## Why the problem is computationally relevant

The aggregation is a textbook *reduction*: a large collection of independent
items must be combined into a small set of totals. This is one of the most
important patterns in high-performance computing because:

* the workload scales **linearly** with data size (O(N)) — doubling the data
  exactly doubles the work;
* the per-item work is small and uniform, so it is an ideal case study for
  the limits of parallelization (memory bandwidth, reduction overhead,
  thread-management cost) rather than for algorithmic cleverness;
* it appears constantly in real analytics pipelines (log aggregation,
  telemetry roll-ups, sensor-data summaries, financial ticks).

## Where SUMO fits in the workflow

SUMO is used **only as a data generator**. We do *not* modify, instrument,
or parallelize SUMO's internal simulation engine. The boundary is explicit:

* **SUMO's job:** simulate traffic, emit per-vehicle trip data
  (`tripinfo.xml`) and floating-car data (`fcd.xml`).
* **Our job:** convert that output to CSV (`tools/sumo_tripinfo_to_csv.py`)
  and run the C++/OpenMP analysis on the resulting dataset.

Because the analyzer consumes a plain CSV, it works identically on:

* **real SUMO output** (the `sumo/` scenario in this repository), and
* **deterministic synthetic datasets** (`tools/generate_dataset.cpp`), which
  exist so the whole experiment is reproducible on machines without SUMO.

## Why large SUMO datasets benefit from parallel processing

A single tripinfo record is tiny, but a realistic city-scale simulation can
emit tens of millions of vehicle records. At 10M records the serial
aggregation reads ~300 MB of in-memory data and performs tens of millions of
floating-point operations. Although the per-record work is small, the
*aggregate* work is large enough that:

* spreading the read+add work across cores can shorten wall-clock time, and
* the experiment exposes the real-world ceiling: beyond a few cores the
  kernel is **memory-bandwidth-bound**, so adding threads stops helping.

That last point is precisely the scientifically interesting result, and it
is why we benchmark rather than assume linear speedup.

## Environment used for the measurements in this repository

| Component | Value |
|-----------|-------|
| CPU       | Apple M2 (arm64), 8 logical cores |
| RAM       | 16 GiB |
| OS        | macOS 26.5.2 (Darwin 25) |
| Compiler  | Homebrew LLVM clang 22.1.8 (`-O2`, `-std=c++17`, `-fopenmp`) |
| OpenMP    | libomp 22.1.8 (via Homebrew LLVM) |
| SUMO      | Eclipse SUMO 1.27.1 (pip `eclipse-sumo`, arm64) |
| Python    | 3.14 (stdlib only for benchmark/plots) |

> **Reproducibility note.** The exact numbers depend on hardware. The
> repository's `benchmarks/results/` files were *measured on the machine
> above*; re-running `python3 benchmarks/run_benchmark.py` on a different
> machine will produce different absolute times but the same qualitative
> behaviour.
