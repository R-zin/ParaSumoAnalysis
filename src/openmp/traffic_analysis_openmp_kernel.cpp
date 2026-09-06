// traffic_analysis_openmp_kernel.cpp
//
// Module II — OpenMP implementation of the analysis kernel.
//
// Parallelization strategy
// ------------------------
// The aggregation loop over the vehicle array is embarrassingly parallel:
// every iteration reads exactly one vehicle record and never writes to any
// per-vehicle state, so iterations are independent and may execute in any
// order, on any number of threads.
//
// The shared aggregates are handled without any manual locking:
//
//   * Sum aggregates (total_speed, total_travel_time, total_waiting_time,
//     total_distance, total_delay_ratio) use OpenMP `reduction(+:...)`.
//     Each thread accumulates into a private copy; the runtime combines the
//     partial sums once, at the end of the parallel region. This removes
//     the race condition that a naive `total += ...` on a shared variable
//     would have.
//
//   * Maxima (max_travel_time, max_waiting_time) use `reduction(max:...)`.
//
//   * The speed histogram cannot be expressed as an OpenMP reduction (the
//     reduction clause only supports scalar variables). Each thread keeps a
//     private histogram on its own stack; after the `for` loop's implicit
//     barrier, every thread atomically adds its 7 partial counts into the
//     shared histogram — 7 atomic adds per thread in total, negligible next
//     to the N loop iterations.
//
// Floating-point note: the reduction changes the order of additions, and
// floating-point addition is not associative. Results can therefore differ
// from the serial version by a few ulps per aggregate. That is expected and
// is exactly why result verification uses an explicit absolute tolerance
// (see kResultTolerance in traffic_analysis.hpp), not bitwise equality.
//
// Correctness invariants (no data races by construction):
//   - the `vehicles` array is only ever read inside the parallel region;
//   - every variable written inside the loop is either loop-local or listed
//     in a reduction clause;
//   - the histogram merge is fully atomic (one `#pragma omp atomic` per
//     bucket per thread), so no two threads corrupt the shared histogram.

#include <omp.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "traffic_analysis.hpp"

namespace traffic {

AnalysisResult analyze_openmp(const std::vector<Vehicle>& vehicles,
                              int num_threads,
                              int* threads_used) {
    AnalysisResult result;
    result.vehicle_count = vehicles.size();

    // Local reduction targets: using locals (instead of the struct fields)
    // keeps the reduction clause simple and standards-conformant.
    double total_speed = 0.0;
    double total_travel_time = 0.0;
    double total_waiting_time = 0.0;
    double total_distance = 0.0;
    double total_delay_ratio = 0.0;
    double max_travel_time = 0.0;
    double max_waiting_time = 0.0;

    const std::size_t n = vehicles.size();
    int team_size_report = 0;

    // `if (num_threads > 0)` lets callers pass 0 (or negative) to mean
    // "let the OpenMP runtime pick the team size" without an invalid
    // num_threads(0) clause.
#pragma omp parallel if (num_threads > 0) num_threads(num_threads)  \
    default(none)                                                   \
    shared(vehicles, n, result, team_size_report)                   \
    firstprivate(num_threads)                                       \
    reduction(+ : total_speed, total_travel_time, total_waiting_time, \
                  total_distance, total_delay_ratio)                \
    reduction(max : max_travel_time, max_waiting_time)
    {
        // Per-thread partial histogram (stack-allocated, never shared).
        std::array<std::uint64_t, 7> local_histogram{};

#pragma omp single
        {
            // Record the actual team size once, from inside the region.
            team_size_report = omp_get_num_threads();
        }
        // NOTE: `single` has an implicit barrier; that is harmless here.

        // Static scheduling is the right default: every iteration performs
        // identical work (a handful of FLOPs on one record), so there is no
        // load imbalance that would justify dynamic-scheduling overhead.
#pragma omp for schedule(static)
        for (std::size_t i = 0; i < n; ++i) {
            const Vehicle& v = vehicles[i];

            total_speed += v.speed;
            total_travel_time += v.travel_time;
            total_waiting_time += v.waiting_time;
            total_distance += v.distance;

            max_travel_time = std::max(max_travel_time, v.travel_time);
            max_waiting_time = std::max(max_waiting_time, v.waiting_time);

            total_delay_ratio +=
                v.travel_time > 0.0 ? v.waiting_time / v.travel_time : 0.0;

            // Bucket mapping identical to the serial kernel (see
            // traffic_analysis.cpp / traffic_analysis.hpp).
            std::size_t bucket;
            if (v.speed < 5.0) bucket = 0;
            else if (v.speed < 10.0) bucket = 1;
            else if (v.speed < 15.0) bucket = 2;
            else if (v.speed < 20.0) bucket = 3;
            else if (v.speed < 25.0) bucket = 4;
            else if (v.speed < 30.0) bucket = 5;
            else bucket = 6;
            ++local_histogram[bucket];
        }
        // Implicit barrier here: every thread's partial sums (reduction
        // copies) and partial histogram are complete before the merge below.

        // Merge the per-thread histograms: 7 atomic additions per thread.
        for (std::size_t b = 0; b < result.speed_histogram.size(); ++b) {
            const std::uint64_t partial = local_histogram[b];
            if (partial != 0) {
#pragma omp atomic
                result.speed_histogram[b] += partial;
            }
        }
    }

    result.total_speed = total_speed;
    result.total_travel_time = total_travel_time;
    result.total_waiting_time = total_waiting_time;
    result.total_distance = total_distance;
    result.total_delay_ratio = total_delay_ratio;
    result.max_travel_time = max_travel_time;
    result.max_waiting_time = max_waiting_time;

    if (result.vehicle_count > 0) {
        const double count = static_cast<double>(result.vehicle_count);
        result.avg_speed = result.total_speed / count;
        result.avg_travel_time = result.total_travel_time / count;
        result.avg_waiting_time = result.total_waiting_time / count;
        result.avg_distance = result.total_distance / count;
        result.avg_delay_ratio = result.total_delay_ratio / count;
    }

    if (threads_used) *threads_used = team_size_report;
    return result;
}

}  // namespace traffic
