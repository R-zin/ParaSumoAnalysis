// traffic_analysis.cpp
//
// Shared building blocks of the analysis pipeline: CSV loading, the serial
// reference kernel, result comparison, and result formatting. Used by both
// the serial and the OpenMP programs so that the only difference between
// them is the aggregation strategy itself.

#include "traffic_analysis.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

namespace traffic {
namespace {

// Parses a floating-point number with std::from_chars (locale-independent,
// no exceptions, no allocations). Returns false on any parse error or when
// trailing characters remain.
bool parse_double(const std::string& text, double& value) {
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc() && result.ptr == last &&
           std::isfinite(value);
}

bool is_header_line(const std::string& line) {
    const auto pos = line.find_first_not_of(" \t\r\n");
    return pos != std::string::npos && std::isalpha(static_cast<unsigned char>(line[pos]));
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    std::stringstream stream(line);
    while (std::getline(stream, field, ',')) {
        // Trim leading/trailing whitespace so "a, b" and "a,b" behave alike.
        const auto begin = field.find_first_not_of(" \t\r\n");
        const auto end = field.find_last_not_of(" \t\r\n");
        fields.push_back(begin == std::string::npos
                             ? std::string{}
                             : field.substr(begin, end - begin + 1));
    }
    return fields;
}

// Maps a speed to its histogram bucket index (see AnalysisResult).
std::size_t speed_bucket(double speed) {
    if (speed < 5.0) return 0;
    if (speed < 10.0) return 1;
    if (speed < 15.0) return 2;
    if (speed < 20.0) return 3;
    if (speed < 25.0) return 4;
    if (speed < 30.0) return 5;
    return 6;
}

}  // namespace

bool load_csv(const std::string& path,
              std::vector<Vehicle>& vehicles,
              std::size_t& skipped_rows) {
    std::ifstream input(path);
    if (!input) {
        std::cerr << "error: cannot open input file '" << path << "'\n";
        return false;
    }

    vehicles.clear();
    skipped_rows = 0;

    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) continue;
        if (line_number == 1 && is_header_line(line)) continue;

        const std::vector<std::string> fields = split_csv_line(line);
        if (fields.size() != 5) {
            ++skipped_rows;
            continue;
        }

        Vehicle vehicle;
        vehicle.vehicle_id = fields[0];
        const bool numeric_ok =
            parse_double(fields[1], vehicle.speed) &&
            parse_double(fields[2], vehicle.travel_time) &&
            parse_double(fields[3], vehicle.waiting_time) &&
            parse_double(fields[4], vehicle.distance);

        // Reject physically meaningless records instead of silently
        // aggregating them into the statistics.
        const bool domain_ok = vehicle.speed >= 0.0 &&
                               vehicle.travel_time >= 0.0 &&
                               vehicle.waiting_time >= 0.0 &&
                               vehicle.distance >= 0.0;

        if (!numeric_ok || !domain_ok) {
            ++skipped_rows;
            continue;
        }
        vehicles.push_back(std::move(vehicle));
    }

    if (vehicles.empty()) {
        std::cerr << "error: no valid vehicle records found in '" << path << "'\n";
        return false;
    }
    return true;
}

AnalysisResult analyze_serial(const std::vector<Vehicle>& vehicles) {
    AnalysisResult result;
    result.vehicle_count = vehicles.size();

    // Single-pass aggregation. This is the exact kernel that the OpenMP
    // version parallelizes: keep the arithmetic identical.
    for (const Vehicle& v : vehicles) {
        result.total_speed += v.speed;
        result.total_travel_time += v.travel_time;
        result.total_waiting_time += v.waiting_time;
        result.total_distance += v.distance;

        result.max_travel_time = std::max(result.max_travel_time, v.travel_time);
        result.max_waiting_time = std::max(result.max_waiting_time, v.waiting_time);

        result.total_delay_ratio +=
            v.travel_time > 0.0 ? v.waiting_time / v.travel_time : 0.0;

        ++result.speed_histogram[speed_bucket(v.speed)];
    }

    if (result.vehicle_count > 0) {
        const double n = static_cast<double>(result.vehicle_count);
        result.avg_speed = result.total_speed / n;
        result.avg_travel_time = result.total_travel_time / n;
        result.avg_waiting_time = result.total_waiting_time / n;
        result.avg_distance = result.total_distance / n;
        result.avg_delay_ratio = result.total_delay_ratio / n;
    }
    return result;
}

namespace {

// Magnitude-scaled comparison: passes when the absolute difference is within
// `tolerance` times the larger magnitude (with a floor of 1 so that values
// near zero still get a meaningful absolute bound).
bool close_enough(double a, double b, double rel_tolerance) {
    const double scale = std::max({1.0, std::fabs(a), std::fabs(b)});
    return std::fabs(a - b) <= rel_tolerance * scale;
}

void check_field(const char* name, double a, double b, double rel_tolerance,
                 bool& ok, std::ostringstream& report) {
    if (!close_enough(a, b, rel_tolerance)) {
        ok = false;
        report << "  MISMATCH " << name << ": serial=" << a
               << " openmp=" << b << " (rel diff "
               << std::fabs(a - b) / std::max({1.0, std::fabs(a), std::fabs(b)})
               << " > rel tol " << rel_tolerance << ")\n";
    }
}

}  // namespace

bool results_equal(const AnalysisResult& a, const AnalysisResult& b,
                   double rel_tolerance, std::string& report_text) {
    bool ok = true;
    std::ostringstream report;

    if (a.vehicle_count != b.vehicle_count) {
        ok = false;
        report << "  MISMATCH vehicle_count: serial=" << a.vehicle_count
               << " openmp=" << b.vehicle_count << "\n";
    }
    check_field("total_speed", a.total_speed, b.total_speed, rel_tolerance, ok, report);
    check_field("total_travel_time", a.total_travel_time, b.total_travel_time,
                rel_tolerance, ok, report);
    check_field("total_waiting_time", a.total_waiting_time, b.total_waiting_time,
                rel_tolerance, ok, report);
    check_field("total_distance", a.total_distance, b.total_distance,
                rel_tolerance, ok, report);
    check_field("max_travel_time", a.max_travel_time, b.max_travel_time,
                rel_tolerance, ok, report);
    check_field("max_waiting_time", a.max_waiting_time, b.max_waiting_time,
                rel_tolerance, ok, report);
    check_field("total_delay_ratio", a.total_delay_ratio, b.total_delay_ratio,
                rel_tolerance, ok, report);
    if (a.speed_histogram != b.speed_histogram) {
        ok = false;
        report << "  MISMATCH speed_histogram\n";
    }

    report_text = ok ? "  all aggregate fields match within relative tolerance " +
                           std::to_string(rel_tolerance)
                     : report.str();
    return ok;
}

std::string format_result(const AnalysisResult& r) {
    std::ostringstream out;
    out.precision(6);
    out << std::fixed;
    out << "  Vehicles             : " << r.vehicle_count << "\n"
        << "  Total speed          : " << r.total_speed << "\n"
        << "  Average speed        : " << r.avg_speed << " m/s\n"
        << "  Total travel time    : " << r.total_travel_time << " s\n"
        << "  Average travel time  : " << r.avg_travel_time << " s\n"
        << "  Total waiting time   : " << r.total_waiting_time << " s\n"
        << "  Average waiting time : " << r.avg_waiting_time << " s\n"
        << "  Max travel time      : " << r.max_travel_time << " s\n"
        << "  Max waiting time     : " << r.max_waiting_time << " s\n"
        << "  Total distance       : " << r.total_distance << " m\n"
        << "  Average distance     : " << r.avg_distance << " m\n"
        << "  Average delay ratio  : " << r.avg_delay_ratio
        << " (waiting/travel)\n"
        << "  Congestion class     : " << r.congestion_class() << "\n"
        << "  Speed histogram [0-5,5-10,10-15,15-20,20-25,25-30,30+):";
    for (const std::uint64_t count : r.speed_histogram) out << ' ' << count;
    out << "\n";
    return out.str();
}

}  // namespace traffic
