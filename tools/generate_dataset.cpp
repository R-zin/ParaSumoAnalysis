// generate_dataset.cpp
//
// Deterministic synthetic vehicle-dataset generator. Produces the exact CSV
// layout consumed by the analyzers:
//
//   vehicle_id,speed,travel_time,waiting_time,distance
//
// The generator exists so the full serial/OpenMP experiment is reproducible
// on machines without SUMO installed. "Deterministic" means: the same
// vehicle count always produces byte-identical output, on any machine,
// because the pseudo-random stream comes from a fixed-seed 64-bit LCG with
// fixed-seed splitmix64 stream mixing (no rand(), no time, no OS entropy).
//
// The value ranges mimic SUMO tripinfo output for an urban scenario:
//   speed        0.5 .. 33.5 m/s   (city traffic up to ~120 km/h)
//   travel_time  30  .. 1230 s     (short urban trips up to ~20 min)
//   waiting_time 0   .. 60% of travel time
//   distance     consistent with speed * travel_time (plus jitter)
//
// Usage:
//   generate_dataset <vehicle_count> [output.csv]
//   (writes to stdout when no output path is given)

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

// 64-bit linear congruential generator (Numerical Recipes constants).
// Fixed multiplier/increment and explicit state -> platform-independent.
class Lcg {
public:
    explicit Lcg(std::uint64_t seed) : state_(seed) {}

    // Uniform double in [0, 1).
    double uniform() {
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        // Take the top 53 bits for a double in [0,1).
        return static_cast<double>(state_ >> 11) * (1.0 / 9007199254740992.0);
    }

private:
    std::uint64_t state_;
};

// splitmix64: mixes the record index into a stream seed so per-field streams
// are decorrelated while remaining fully deterministic.
std::uint64_t mix(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: " << argv[0] << " <vehicle_count> [output.csv]\n";
        return 1;
    }

    const long long count = std::atoll(argv[1]);
    if (count <= 0) {
        std::cerr << "error: vehicle_count must be positive\n";
        return 1;
    }

    std::ofstream file;
    std::ostream* out = &std::cout;
    if (argc == 3) {
        file.open(argv[2], std::ios::out | std::ios::trunc);
        if (!file) {
            std::cerr << "error: cannot write to '" << argv[2] << "'\n";
            return 1;
        }
        out = &file;
    }

    // Independent decorrelated streams per field; seeds are constants.
    Lcg speed_rng(mix(101ULL));
    Lcg travel_rng(mix(202ULL));
    Lcg wait_rng(mix(303ULL));
    Lcg jitter_rng(mix(404ULL));

    *out << "vehicle_id,speed,travel_time,waiting_time,distance\n";
    out->setf(std::ios::fixed);
    out->precision(4);

    for (long long i = 0; i < count; ++i) {
        const double speed = 0.5 + speed_rng.uniform() * 33.0;
        const double travel_time = 30.0 + travel_rng.uniform() * 1200.0;
        const double waiting_time =
            wait_rng.uniform() * travel_time * 0.6;
        const double jitter = 0.9 + jitter_rng.uniform() * 0.2;
        const double distance = speed * travel_time * jitter;

        *out << "veh_" << i << ',' << speed << ',' << travel_time << ','
             << waiting_time << ',' << distance << '\n';
    }
    return 0;
}
