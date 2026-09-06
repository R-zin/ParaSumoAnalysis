// test_correctness.cpp
//
// Deterministic, manually verifiable correctness tests for the serial and
// OpenMP analysis kernels.
//
//   Test 1 — the 3-vehicle example from the project specification:
//            every metric is checked against hand-computed values.
//   Test 2 — a larger deterministic pseudo-random dataset: the OpenMP
//            kernel must match the serial kernel within the documented
//            tolerance for team sizes 1, 2, 4, 8 (clamped to hardware).
//   Test 3 — malformed CSV input is rejected gracefully (skipped, not
//            crashed on, and counted).

#include <omp.h>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "traffic_analysis.hpp"

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
    if (condition) {
        std::cout << "  PASS  " << what << "\n";
    } else {
        std::cout << "  FAIL  " << what << "\n";
        ++failures;
    }
}

bool near(double a, double b, double tolerance = 1e-9) {
    return std::fabs(a - b) <= tolerance;
}

// Builds the 3-vehicle example from the project specification.
std::vector<traffic::Vehicle> spec_dataset() {
    return {
        {"V1", 10.0, 100.0, 20.0, 900.0},
        {"V2", 20.0, 80.0, 10.0, 1200.0},
        {"V3", 15.0, 90.0, 15.0, 1000.0},
    };
}

// Hand-computed expectations for the spec dataset (see docs/algorithm.md):
//   count=3, total_speed=45, avg_speed=15, total_travel=270, avg_travel=90,
//   total_wait=45, avg_wait=15, total_distance=3100, avg_distance=1033.33...
void test_spec_dataset() {
    std::cout << "Test 1: 3-vehicle specification example\n";
    const std::vector<traffic::Vehicle> vehicles = spec_dataset();

    const traffic::AnalysisResult serial = traffic::analyze_serial(vehicles);
    expect(serial.vehicle_count == 3, "vehicle_count == 3");
    expect(near(serial.total_speed, 45.0), "total_speed == 45");
    expect(near(serial.avg_speed, 15.0), "avg_speed == 15");
    expect(near(serial.total_travel_time, 270.0), "total_travel_time == 270");
    expect(near(serial.avg_travel_time, 90.0), "avg_travel_time == 90");
    expect(near(serial.total_waiting_time, 45.0), "total_waiting_time == 45");
    expect(near(serial.avg_waiting_time, 15.0), "avg_waiting_time == 15");
    expect(near(serial.total_distance, 3100.0), "total_distance == 3100");
    expect(near(serial.avg_distance, 3100.0 / 3.0, 1e-6),
           "avg_distance == 1033.33...");
    expect(near(serial.max_travel_time, 100.0), "max_travel_time == 100");
    expect(near(serial.max_waiting_time, 20.0), "max_waiting_time == 20");

    // OpenMP with every supported team size must match the hand-computed
    // values too (this is the strongest race detector on a tiny input).
    const int max_threads = omp_get_num_procs();
    for (int threads : {1, 2, 4, 8}) {
        if (threads > max_threads) continue;
        const traffic::AnalysisResult parallel =
            traffic::analyze_openmp(vehicles, threads);
        std::string report;
        const bool ok = traffic::results_equal(serial, parallel, 1e-9, report);
        expect(ok, "OpenMP(" + std::to_string(threads) +
                       " threads) matches hand-computed values" +
                       (ok ? "" : "\n" + report));
    }
}

// Deterministic LCG so the test needs no external data and never depends on
// wall time or global rand() state.
std::vector<traffic::Vehicle> pseudo_random_dataset(std::size_t count,
                                                    std::uint64_t seed) {
    std::vector<traffic::Vehicle> vehicles;
    vehicles.reserve(count);
    std::uint64_t state = seed;
    auto next = [&state]() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>((state >> 11) & 0xFFFFFFFF) / 4294967295.0;
    };
    for (std::size_t i = 0; i < count; ++i) {
        traffic::Vehicle v;
        v.vehicle_id = "t" + std::to_string(i);
        v.speed = next() * 40.0;
        v.travel_time = 50.0 + next() * 900.0;
        v.waiting_time = next() * v.travel_time * 0.5;
        v.distance = v.speed * v.travel_time;
        vehicles.push_back(v);
    }
    return vehicles;
}

void test_serial_openmp_equivalence() {
    std::cout << "Test 2: serial == OpenMP on 100,000 deterministic records\n";
    const std::vector<traffic::Vehicle> vehicles =
        pseudo_random_dataset(100000, 42);

    const traffic::AnalysisResult serial = traffic::analyze_serial(vehicles);
    const int max_threads = omp_get_num_procs();
    std::cout << "  (hardware reports " << max_threads
              << " processors; testing team sizes up to that limit)\n";

    for (int threads : {1, 2, 4, 8}) {
        if (threads > max_threads) {
            std::cout << "  SKIP  " << threads
                      << " threads (exceeds available hardware)\n";
            continue;
        }
        const traffic::AnalysisResult parallel =
            traffic::analyze_openmp(vehicles, threads);
        std::string report;
        const bool ok = traffic::results_equal(
            serial, parallel, traffic::kResultTolerance, report);
        expect(ok, "OpenMP(" + std::to_string(threads) +
                       ") == serial within tolerance " +
                       std::to_string(traffic::kResultTolerance) +
                       (ok ? "" : "\n" + report));
    }
}

void test_malformed_csv() {
    std::cout << "Test 3: malformed CSV rows are skipped gracefully\n";
    const char* path = "test_malformed_tmp.csv";
    {
        std::FILE* f = std::fopen(path, "w");
        std::fputs("vehicle_id,speed,travel_time,waiting_time,distance\n", f);
        std::fputs("V1,10,100,20,900\n", f);          // valid
        std::fputs("V2,abc,80,10,1200\n", f);         // non-numeric
        std::fputs("V3,15,90\n", f);                  // too few fields
        std::fputs("V4,-5,90,15,1000\n", f);          // negative speed
        std::fputs("V5,15,90,15,1000,extra\n", f);    // too many fields
        std::fputs("V6,20,80,10,1200\n", f);          // valid
        std::fclose(f);
    }

    std::vector<traffic::Vehicle> vehicles;
    std::size_t skipped = 0;
    const bool loaded = traffic::load_csv(path, vehicles, skipped);
    std::remove(path);

    expect(loaded, "file with 2 valid rows loads successfully");
    expect(vehicles.size() == 2, "exactly 2 valid rows kept");
    expect(skipped == 4, "exactly 4 malformed rows skipped");
    if (vehicles.size() == 2) {
        expect(vehicles[0].vehicle_id == "V1" && vehicles[1].vehicle_id == "V6",
               "valid rows are V1 and V6, in order");
    }

    // A file with zero valid rows must fail cleanly, not crash.
    {
        std::FILE* f = std::fopen(path, "w");
        std::fputs("vehicle_id,speed,travel_time,waiting_time,distance\n", f);
        std::fputs("bad,row,here,nope,wrong\n", f);
        std::fclose(f);
    }
    const bool loaded_empty = traffic::load_csv(path, vehicles, skipped);
    std::remove(path);
    expect(!loaded_empty, "file with zero valid rows is rejected");
}

}  // namespace

int main() {
    std::cout << "=== SumoPara correctness tests ===\n\n";
    test_spec_dataset();
    std::cout << "\n";
    test_serial_openmp_equivalence();
    std::cout << "\n";
    test_malformed_csv();

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED"
                                        : std::to_string(failures) +
                                              " TEST(S) FAILED")
              << "\n";
    return failures == 0 ? 0 : 1;
}
