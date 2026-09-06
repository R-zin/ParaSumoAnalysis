# SUMO integration details

This document explains how SUMO connects to the analyzer and how the
repository's scenario was produced and validated. See `sumo/README.md` for
the runnable commands.

## Boundary of responsibility

```
┌─────────────┐   tripinfo.xml / fcd.xml   ┌──────────────────────┐
│    SUMO     │ ─────────────────────────► │  SumoPara analyzer   │
│ (simulator) │                            │  (serial + OpenMP)   │
└─────────────┘                            └──────────────────────┘
   unchanged,                                   our code:
   unmodified                              CSV → statistics → benchmark
```

SUMO is used **only** to generate data. Nothing in SUMO's engine is modified
or parallelized; the parallelization target is the analysis loop over the
exported records.

## Scenario provenance (validated on this machine)

The committed `sumo/network.net.xml` was produced with the real tools, not
hand-written:

```
netgenerate --grid --grid.number=3 --grid.length=200 \
            --default.speed=13.89 --tls.guess \
            --output-file=sumo/network.net.xml
netconvert -s sumo/network.net.xml --plain-output-prefix=/tmp/netcheck   # validates
sumo -c sumo/simulation.sumocfg                                          # runs
```

Validated with **Eclipse SUMO 1.27.1** (macOS arm64, pip `eclipse-sumo`):

* `netgenerate` exit 0, `netconvert` reported the network valid;
* the simulation completed and emitted **945 completed trips**;
* `tools/sumo_tripinfo_to_csv.py` converted all 945 records, 0 malformed;
* both analyzers processed the CSV and agreed within tolerance.

### Why the demand is congested on purpose

The first scenario draft used light demand and produced `waitingTime = 0` for
every vehicle — a useless demo. `routes.rou.xml` was therefore given four
flows that converge on the two centre junctions so the guessed traffic lights
become a bottleneck. The measured result: average waiting ≈ 9.45 s, max
waiting ≈ 1050 s, congestion class **SLOW**. This exercises the waiting/delay
metrics for real.

## Reproducing without SUMO

The analyzer reads plain CSV, so nothing about the experiment depends on
SUMO. `tools/generate_dataset.cpp` emits deterministic, value-range-realistic
records (speed 0.5–33.5 m/s, travel 30–1230 s, waiting ≤ 60% of travel,
distance consistent with speed×time). The 10K/100K/1M/5M/10M benchmark
datasets are all synthetic and byte-for-byte reproducible.
