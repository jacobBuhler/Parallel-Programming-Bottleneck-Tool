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

struct ProjectedScalingResult {
    int threads;
    double predicted_speedup;
    double predicted_time;
    double predicted_efficiency;
};

std::vector<ScalingResult> build_scaling_results(
    const std::vector<int>& threads,
    const std::vector<std::vector<double>>& all_times
);

std::vector<int> build_default_projection_threads(const std::vector<ScalingResult>& results);

std::vector<ProjectedScalingResult> build_projected_scaling_results(
    const std::vector<ScalingResult>& measured_results,
    const std::vector<int>& projected_threads
);

void print_scaling_summary(const std::vector<ScalingResult>& results);

void print_projected_scaling_summary(
    const std::vector<ProjectedScalingResult>& projected_results,
    double serial_fraction_used
);

void print_diagnosis(const std::vector<ScalingResult>& results);

void write_diagnosis_report(
    const std::vector<ScalingResult>& results,
    const std::vector<ProjectedScalingResult>& projected_results,
    double serial_fraction_used,
    const std::string& filename
);
#endif
