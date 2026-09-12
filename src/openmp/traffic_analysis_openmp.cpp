// traffic_analysis_openmp.cpp
//
// Module II — OpenMP implementation of the SUMO traffic-data analysis.
// Produces results equivalent to the serial version (verified with an
// explicit floating-point tolerance, see --verify) and reports pure
// computation time; file I/O is excluded from the benchmark exactly as in
// the serial program.
//
// Usage:
//   traffic_analysis_openmp <input.csv> [--threads N] [--bench]
//                           [--iterations N] [--verify]

#include <omp.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "traffic_analysis.hpp"

namespace {

struct Options {
    std::string input_path;
    int threads = 0;        // 0 = OpenMP runtime default
    bool benchmark = false;
    int iterations = 1;
    bool verify = false;    // --verify: compare against the serial kernel
};

bool parse_args(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--bench") {
            options.benchmark = true;
        } else if (arg == "--verify") {
            options.verify = true;
        } else if (arg == "--threads" && i + 1 < argc) {
            options.threads = std::atoi(argv[++i]);
            if (options.threads < 0) options.threads = 0;
        } else if (arg == "--iterations" && i + 1 < argc) {
            options.iterations = std::atoi(argv[++i]);
            if (options.iterations < 1) options.iterations = 1;
        } else if (arg == "--help" || arg == "-h") {
            return false;
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return false;
        } else if (options.input_path.empty()) {
            options.input_path = arg;
        } else {
            std::cerr << "error: unexpected extra argument '" << arg << "'\n";
            return false;
        }
    }
    return !options.input_path.empty();
}

void print_usage(const char* program) {
    std::cerr << "usage: " << program
              << " <input.csv> [--threads N] [--bench] [--iterations N]"
                 " [--verify]\n";
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_args(argc, argv, options)) {
        print_usage(argv[0]);
        return 1;
    }

    // ---- Input phase (excluded from the benchmark) -----------------------
    std::vector<traffic::Vehicle> vehicles;
    std::size_t skipped_rows = 0;
    if (!traffic::load_csv(options.input_path, vehicles, skipped_rows)) {
        return 1;
    }

    // Report the hardware actually available instead of assuming a fixed
    // thread count (omp_get_num_procs = processors visible to the process).
    std::cout << "Input: " << options.input_path << "\n"
              << "Loaded " << vehicles.size() << " vehicle records";
    if (skipped_rows > 0) {
        std::cout << " (" << skipped_rows << " malformed rows skipped)";
    }
    std::cout << "\nHardware: " << omp_get_num_procs()
              << " processors available to OpenMP (max threads: "
              << omp_get_max_threads() << ")\n";

    // ---- Computation phase (benchmarked) ---------------------------------
    traffic::AnalysisResult result;
    int threads_used = 0;
    double best_seconds = std::numeric_limits<double>::max();
    double total_seconds = 0.0;
    for (int iter = 0; iter < options.iterations; ++iter) {
        const double start = omp_get_wtime();
        result = traffic::analyze_openmp(vehicles, options.threads,
                                         &threads_used);
        const double seconds = omp_get_wtime() - start;
        best_seconds = std::min(best_seconds, seconds);
        total_seconds += seconds;
    }

    // ---- Output phase ----------------------------------------------------
    std::cout << "\nOpenMP analysis results (team size: " << threads_used
              << "):\n"
              << traffic::format_result(result);

    // ---- Optional verification against the serial kernel -----------------
    if (options.verify) {
        const traffic::AnalysisResult reference =
            traffic::analyze_serial(vehicles);
        std::string report;
        const bool pass = traffic::results_equal(
            reference, result, traffic::kResultTolerance, report);
        std::cout << "\nVerification vs serial (relative tolerance "
                  << traffic::kResultTolerance << "): "
                  << (pass ? "PASS" : "FAIL") << "\n"
                  << report << "\n";
        if (!pass) return 2;
    }

    if (options.benchmark) {
        // Machine-readable line for the benchmark harness:
        //   BENCH openmp threads=T iterations=N best_s=... mean_s=...
        std::cout << "\nBENCH openmp threads=" << threads_used
                  << " iterations=" << options.iterations
                  << " best_s=" << best_seconds
                  << " mean_s=" << total_seconds / options.iterations << "\n";
    }
    return 0;
}
