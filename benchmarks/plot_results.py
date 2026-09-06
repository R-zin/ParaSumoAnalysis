#!/usr/bin/env python3
"""Plot benchmark results as self-contained SVG files (no third-party
dependencies, so it runs on any stock Python 3).

Reads benchmarks/results/benchmark_results.csv and writes:
    graphs/speedup_vs_threads.svg      (with ideal linear-speedup reference)
    graphs/time_vs_dataset_size.svg    (log-log)

Usage:
    python3 benchmarks/plot_results.py [results.csv] [graphs_dir]
"""

import csv
import math
import os
import sys

W, H = 760, 480
PAD_L, PAD_R, PAD_T, PAD_B = 70, 20, 40, 60
COLORS = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b"]


def nice_ticks(vmin, vmax, count=6):
    if vmax <= vmin:
        vmax = vmin + 1
    span = vmax - vmin
    step = 10 ** round(math.log10(span / count))
    for mult in (1, 2, 5, 10):
        if span / (step * mult) <= count:
            step *= mult
            break
    lo = math.floor(vmin / step) * step
    hi = math.ceil(vmax / step) * step
    ticks, v = [], lo
    while v <= hi + 1e-12:
        ticks.append(round(v, 10))
        v += step
    return ticks, lo, hi


def svg_header():
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
            f'viewBox="0 0 {W} {H}" font-family="Menlo, monospace" '
            f'font-size="12" fill="#222">\n'
            f'<rect width="{W}" height="{H}" fill="white"/>\n')


def line_plot(title, x_label, y_label, series, x_values, y_range=None,
              log_x=False, ideal=None):
    """series: list of (name, color, [y per x]). ideal: optional (name, [y])."""
    plot_w, plot_h = W - PAD_L - PAD_R, H - PAD_T - PAD_B

    xs = [math.log10(x) for x in x_values] if log_x else list(x_values)
    x_min, x_max = min(xs), max(xs)
    if x_max == x_min:
        x_max = x_min + 1

    all_y = [y for _, _, ys in series for y in ys if y is not None]
    if ideal:
        all_y += ideal[1]
    y_lo = 0.0
    y_hi = max(all_y) * 1.05 if all_y else 1.0
    if y_range:
        y_lo, y_hi = y_range

    def sx(x):
        return PAD_L + (x - x_min) / (x_max - x_min) * plot_w

    def sy(y):
        return PAD_T + plot_h - (y - y_lo) / (y_hi - y_lo) * plot_h

    out = [svg_header()]
    out.append(f'<text x="{W/2}" y="22" text-anchor="middle" font-size="15" '
               f'font-weight="bold">{title}</text>')

    # Axes.
    out.append(f'<line x1="{PAD_L}" y1="{PAD_T}" x2="{PAD_L}" '
               f'y2="{PAD_T+plot_h}" stroke="#333"/>')
    out.append(f'<line x1="{PAD_L}" y1="{PAD_T+plot_h}" '
               f'x2="{PAD_L+plot_w}" y2="{PAD_T+plot_h}" stroke="#333"/>')

    # Y ticks.
    ticks, _, _ = nice_ticks(y_lo, y_hi)
    for t in ticks:
        if not (y_lo <= t <= y_hi):
            continue
        y = sy(t)
        out.append(f'<line x1="{PAD_L}" y1="{y:.1f}" x2="{PAD_L+plot_w}" '
                   f'y2="{y:.1f}" stroke="#e4e4e4"/>')
        out.append(f'<text x="{PAD_L-6}" y="{y+4:.1f}" text-anchor="end">'
                   f'{t:g}</text>')

    # X ticks at the actual data points (thread counts / dataset sizes).
    for xv, raw in zip(xs, x_values):
        x = sx(xv)
        out.append(f'<line x1="{x:.1f}" y1="{PAD_T+plot_h}" x2="{x:.1f}" '
                   f'y2="{PAD_T+plot_h+5}" stroke="#333"/>')
        out.append(f'<text x="{x:.1f}" y="{PAD_T+plot_h+20}" '
                   f'text-anchor="middle">{raw:g}</text>')

    out.append(f'<text x="{PAD_L+plot_w/2}" y="{H-10}" text-anchor="middle">'
               f'{x_label}</text>')
    out.append(f'<text x="16" y="{PAD_T+plot_h/2}" text-anchor="middle" '
               f'transform="rotate(-90 16 {PAD_T+plot_h/2})">{y_label}</text>')

    # Ideal reference line.
    if ideal:
        pts = " ".join(f"{sx(x):.1f},{sy(y):.1f}"
                       for x, y in zip(xs, ideal[1]))
        out.append(f'<polyline points="{pts}" fill="none" stroke="#888" '
                   f'stroke-dasharray="5,4" stroke-width="1.5"/>')
        out.append(f'<text x="{sx(xs[-1])-4:.1f}" y="{sy(ideal[1][-1])-8:.1f}" '
                   f'text-anchor="end" fill="#666">{ideal[0]}</text>')

    # Series + legend.
    for i, (name, color, ys) in enumerate(series):
        pts = [(sx(x), sy(y)) for x, y in zip(xs, ys) if y is not None]
        out.append('<polyline points="' +
                   " ".join(f"{x:.1f},{y:.1f}" for x, y in pts) +
                   f'" fill="none" stroke="{color}" stroke-width="2"/>')
        for x, y in pts:
            out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3" '
                       f'fill="{color}"/>')
        lx, ly = PAD_L + 12 + i * 150, PAD_T + 14
        out.append(f'<rect x="{lx}" y="{ly-9}" width="10" height="10" '
                   f'fill="{color}"/>')
        out.append(f'<text x="{lx+14}" y="{ly}">{name}</text>')

    out.append('</svg>\n')
    return "".join(out)


def load_rows(csv_path):
    rows = []
    with open(csv_path) as f:
        for r in csv.DictReader(f):
            rows.append(r)
    return rows


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.join(os.path.dirname(__file__), "results",
                     "benchmark_results.csv")
    graphs_dir = sys.argv[2] if len(sys.argv) > 2 else \
        os.path.join(os.path.dirname(__file__), "graphs")
    os.makedirs(graphs_dir, exist_ok=True)

    rows = load_rows(csv_path)
    datasets = sorted({r["dataset"] for r in rows},
                      key=lambda d: int(float(d.replace("K", "e3")
                                            .replace("M", "e6"))))
    thread_counts = sorted({int(r["threads"]) for r in rows})

    def cell(dataset, threads, field):
        for r in rows:
            if r["dataset"] == dataset and int(r["threads"]) == threads:
                return float(r[field])
        return None

    # ---- speedup vs threads (vs serial baseline) ----------------------------
    series = []
    for i, dataset in enumerate(datasets):
        ys = [cell(dataset, t, "speedup_vs_serial") for t in thread_counts]
        series.append((dataset, COLORS[i % len(COLORS)], ys))
    ideal = ("ideal linear", [float(t) for t in thread_counts])
    svg = line_plot("Speedup vs threads (measured, serial baseline)",
                    "threads", "speedup S(p) = T(serial)/T(p)",
                    series, thread_counts, ideal=ideal)
    with open(os.path.join(graphs_dir, "speedup_vs_threads.svg"), "w") as f:
        f.write(svg)

    # ---- execution time vs dataset size (log-log) --------------------------
    sizes = []
    for d in datasets:
        s = d.replace("K", "e3").replace("M", "e6")
        sizes.append(float(s))
    series = [("serial", "#555555",
               [cell(d, 1, "serial_time_s") for d in datasets])]
    for i, t in enumerate(thread_counts[1:], start=1):
        series.append((f"{t} threads", COLORS[(i - 1) % len(COLORS)],
                       [cell(d, t, "time_median_s") for d in datasets]))
    svg = line_plot("Computation time vs dataset size (log x)", "vehicles",
                    "time (s)", series, sizes, log_x=True)
    with open(os.path.join(graphs_dir, "time_vs_dataset_size.svg"), "w") as f:
        f.write(svg)

    # ---- efficiency vs threads ---------------------------------------------
    series = []
    for i, dataset in enumerate(datasets):
        ys = [cell(dataset, t, "efficiency_vs_serial") for t in thread_counts]
        series.append((dataset, COLORS[i % len(COLORS)], ys))
    svg = line_plot("Parallel efficiency vs threads (measured, serial baseline)",
                    "threads", "efficiency E(p) = S(p)/p",
                    series, thread_counts, y_range=(0, 1.3))
    with open(os.path.join(graphs_dir, "efficiency_vs_threads.svg"), "w") as f:
        f.write(svg)

    print(f"graphs written to {graphs_dir}")


if __name__ == "__main__":
    main()
