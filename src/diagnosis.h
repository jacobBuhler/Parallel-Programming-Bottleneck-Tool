#ifndef DIAGNOSIS_H
#define DIAGNOSIS_H

#include <vector>
#include <string>

struct ScalingResult {
    int threads;
    double avg_time;
    double min_time;
    double max_time;
    double stddev_time;
    double speedup;
    double ideal_speedup;
    double efficiency;
    double serial_fraction;
    double max_theoretical_speedup;
};

std::vector<ScalingResult> build_scaling_results(
    const std::vector<int>& threads,
    const std::vector<std::vector<double>>& all_times
);

void print_scaling_summary(const std::vector<ScalingResult>& results);

void print_diagnosis(const std::vector<ScalingResult>& results);

void write_diagnosis_report(
    const std::vector<ScalingResult>& results,
    const std::string& filename
);

#endif
