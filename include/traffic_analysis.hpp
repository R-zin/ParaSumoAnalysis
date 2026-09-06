// traffic_analysis.hpp
//
// Shared data model and declarations for the SUMO traffic-data analysis
// project. Both the serial and the OpenMP implementations use these types so
// that they operate on identical inputs and produce comparable outputs.
//
// Analysis kernel contract (shared by both implementations):
//   - aggregate sums  : computed with a reduction over all vehicles
//   - maxima          : computed with a max-reduction over all vehicles
//   - histogram       : computed with per-worker partial histograms that are
//                       merged after the main loop (serial version: one worker)
//   - averages        : derived after the loop as sum / count
//
// Keeping this contract identical is what makes serial-vs-parallel result
// verification meaningful: the only permitted source of divergence is the
// (documented) non-associativity of floating-point summation order.

#ifndef SUMOPARA_TRAFFIC_ANALYSIS_HPP
#define SUMOPARA_TRAFFIC_ANALYSIS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace traffic {

// One vehicle record, as exported from SUMO (tripinfo / fcd output) or as
// produced by the deterministic synthetic dataset generator.
//
// CSV column order (with a header row):
//   vehicle_id,speed,travel_time,waiting_time,distance
struct Vehicle {
    std::string vehicle_id;  // kept for traceability with SUMO output
    double speed = 0.0;        // mean speed over the trip [m/s]
    double travel_time = 0.0;  // total trip duration [s]
    double waiting_time = 0.0; // time spent (nearly) stopped [s]
    double distance = 0.0;     // route length travelled [m]
};

// Complete result of one analysis run over a vehicle dataset.
struct AnalysisResult {
    std::size_t vehicle_count = 0;

    double total_speed = 0.0;
    double total_travel_time = 0.0;
    double total_waiting_time = 0.0;
    double total_distance = 0.0;

    double max_travel_time = 0.0;
    double max_waiting_time = 0.0;

    // Derived (computed after aggregation, identical in both versions).
    double avg_speed = 0.0;
    double avg_travel_time = 0.0;
    double avg_waiting_time = 0.0;
    double avg_distance = 0.0;

    // Delay: waiting_time / travel_time, summed per vehicle then averaged.
    // A vehicle with travel_time == 0 contributes a ratio of 0 (the kernel
    // guards the division in both implementations identically).
    double total_delay_ratio = 0.0;
    double avg_delay_ratio = 0.0;

    // Per-vehicle speed histogram, used for congestion classification.
    // Buckets (m/s): [0,5) [5,10) [10,15) [15,20) [20,25) [25,30) [30,inf)
    std::array<std::uint64_t, 7> speed_histogram{};

    // Congestion classification thresholds on average speed [m/s].
    static constexpr double kCongestedThreshold = 5.0;
    static constexpr double kSlowThreshold = 15.0;

    std::string congestion_class() const {
        if (avg_speed < kCongestedThreshold) return "CONGESTED";
        if (avg_speed < kSlowThreshold) return "SLOW";
        return "FREE-FLOW";
    }
};

// Relative tolerance used by the checker / verification tests when comparing
// serial results against OpenMP results. Two aggregates a and b compare
// equal when |a - b| <= kResultTolerance * max(1, |a|, |b|), i.e. the
// tolerance scales with the magnitude of the value being compared.
//
// Rationale: the parallel reduction reorders floating-point additions and
// addition is not associative, so tiny differences are *expected*. Their
// size grows with the magnitude of the sums (for a 1e6-row dataset the
// totals reach ~1e8..1e10, where even a perfect reordering differs by
// ~1e-5 absolute). A fixed absolute tolerance therefore either misses real
// bugs (if loose) or fails on correct code (if tight). The magnitude-scaled
// form with 1e-9 is roughly 4 orders of magnitude looser than the observed
// reordering noise (~1e-13 relative) while still catching any genuine bug —
// a data race or logic error corrupts results by far more than 1e-9 relative.
constexpr double kResultTolerance = 1e-9;

// Loads vehicle records from a CSV file with the column layout documented on
// `Vehicle`. Malformed rows (wrong field count, non-numeric fields, negative
// values) are skipped and counted. Returns true on success; on failure
// (unreadable file, or zero valid rows) returns false and writes a message to
// stderr. The header line is detected and skipped automatically.
bool load_csv(const std::string& path,
              std::vector<Vehicle>& vehicles,
              std::size_t& skipped_rows);

// Serial reference implementation of the analysis kernel.
AnalysisResult analyze_serial(const std::vector<Vehicle>& vehicles);

// OpenMP implementation of the same kernel. The `num_threads` argument
// (values > 0) requests a fixed team size via `num_threads(...)`; a value
// <= 0 leaves the decision to the OpenMP runtime. Returns the requested team
// size (0 = runtime default) through `threads_used` when non-null.
AnalysisResult analyze_openmp(const std::vector<Vehicle>& vehicles,
                              int num_threads,
                              int* threads_used = nullptr);

// Compares two results field by field. Returns true when every numeric
// aggregate differs by at most `tolerance` (absolute) and the count and
// histogram match exactly. Differences are reported in human-readable form.
bool results_equal(const AnalysisResult& a,
                   const AnalysisResult& b,
                   double tolerance,
                   std::string& report);

// Formats a result as aligned key/value lines for console output.
std::string format_result(const AnalysisResult& result);

}  // namespace traffic

#endif  // SUMOPARA_TRAFFIC_ANALYSIS_HPP
