#ifndef DIAGNOSIS_H
#define DIAGNOSIS_H

#include <vector>
#include <string>

struct MachineInfo {
    std::string cpu_model;
    int physical_cores = 0;
    int logical_cores  = 0;
    long total_ram_mb   = 0;
    std::string kernel;
    std::string compiler;
};
 
//one final verdict of program
struct OverallVerdict {
    std::string label;
    std::string explanation;//written explaination
    std::string confidence;//confidence level of verdict
};

struct RusageMetrics {
    double user_time   = 0.0; //cpu time in user mode
    double system_time = 0.0;//cpu time spent in ckernel mode
    long max_rss = 0;//peak resident set size
    long voluntary_ctx_switches = 0; //child yielded on its own
    long involuntary_ctx_switches = 0;//scheduler preempted child
    long minor_page_faults = 0;
    long major_page_faults = 0;
};

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

    RusageMetrics rusage;
    double parallel_util = 0.0;
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

void attach_rusage_to_results(
    std::vector<ScalingResult>& results,
    const std::vector<std::vector<RusageMetrics>>& all_rusage
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

//inspect the host machine
MachineInfo detect_machine_info();
//print machine info
void print_machine_info(const MachineInfo& m);
//gather an overall verdict
OverallVerdict compute_overall_verdict(
    const std::vector<ScalingResult>& results,
    const MachineInfo& machine
);

void print_overall_verdict(const OverallVerdict& v);

void print_diagnosis(
    const std::vector<ScalingResult>& results,
    const MachineInfo& machine
);

void write_diagnosis_report(
    const std::vector<ScalingResult>& results,
    const std::vector<ProjectedScalingResult>& projected_results,
    double serial_fraction_used,
    const MachineInfo& machine,
    const OverallVerdict& overall,
    const std::string& filename
);
#endif