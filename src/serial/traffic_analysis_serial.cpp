// traffic_analysis_serial.cpp
//
// Module I — serial reference implementation of the SUMO traffic-data
// analysis. Loads vehicle records from CSV, aggregates traffic statistics
// in a single O(N) pass, prints the results, and optionally reports the
// pure computation time (file I/O is never included in the benchmark).
//
// Usage:
//   traffic_analysis_serial <input.csv> [--bench] [--iterations N]

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "traffic_analysis.hpp"

namespace {

struct Options {
    std::string input_path;
    bool benchmark = false;      // --bench: print computation-time line
    int iterations = 1;          // --iterations N: repeat analysis N times
};

bool parse_args(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--bench") {
            options.benchmark = true;
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
              << " <input.csv> [--bench] [--iterations N]\n";
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

    std::cout << "Input: " << options.input_path << "\n"
              << "Loaded " << vehicles.size() << " vehicle records";
    if (skipped_rows > 0) {
        std::cout << " (" << skipped_rows << " malformed rows skipped)";
    }
    std::cout << "\n\nSerial analysis results:\n";

    // ---- Computation phase (this is what gets benchmarked) ---------------
    //
    // The loop below repeats the analysis `options.iterations` times so the
    // benchmark script can amortize timer noise. With 1 iteration (default)
    // this behaves exactly like a plain one-shot analysis.
    traffic::AnalysisResult result;
    double best_seconds = std::numeric_limits<double>::max();
    double total_seconds = 0.0;
    for (int iter = 0; iter < options.iterations; ++iter) {
        const auto start = std::chrono::high_resolution_clock::now();
        result = traffic::analyze_serial(vehicles);
        const auto stop = std::chrono::high_resolution_clock::now();
        const double seconds =
            std::chrono::duration<double>(stop - start).count();
        best_seconds = std::min(best_seconds, seconds);
        total_seconds += seconds;
    }

    // ---- Output phase ----------------------------------------------------
    std::cout << traffic::format_result(result);

    if (options.benchmark) {
        // Machine-readable line for the benchmark harness:
        //   BENCH serial threads=1 iterations=N best=... mean=...
        std::cout << "\nBENCH serial threads=1 iterations=" << options.iterations
                  << " best_s=" << best_seconds
                  << " mean_s=" << total_seconds / options.iterations << "\n";
    }
    return 0;
}
