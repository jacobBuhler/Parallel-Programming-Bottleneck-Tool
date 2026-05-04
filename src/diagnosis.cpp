//diagnosis.cpp
#include "diagnosis.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <limits>
#include <algorithm>
#include <cmath>
#include <set>
#include <cmath>
#include <cstdio>
#include <thread>
#include <sys/utsname.h>

static double compute_average(const std::vector<double>& values){//computes average of a vector of timing values
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for(double v: values){
        sum += v;
    }
    return sum / static_cast<double>(values.size());
}

static double compute_min(const std::vector<double>& values){//returns min
    if(values.empty()) return 0.0;
    return *std::min_element(values.begin(), values.end());
}

static double compute_max(const std::vector<double>& values){//returns max
    if(values.empty()) return 0.0;
    return *std::max_element(values.begin(), values.end());
}

static double compute_stddev(const std::vector<double>& values, double mean){//returns standard deviation
    if(values.size() <= 1) return 0.0;

    double sum_sq = 0.0;
    for(double v : values){
        double diff = v - mean;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / static_cast<double>(values.size()));
}

static double estimate_serial_fraction(double speedup, int threads){//estimates serial fraction using Amdahl's law
//tells the user how much of their code behaves sequentially 
    if(threads <= 1 || speedup <= 0.0){
        return 0.0;
    }
    double numerator = (1.0 / speedup) - (1.0 / threads);
    double denominator = 1.0 - (1.0 / threads);

    if(denominator == 0.0){
        return 0.0;
    }

    double f = numerator / denominator;

    if(f < 0.0) f = 0.0;
    if(f > 1.0) f = 1.0;

    return f;
}

static double predict_speedup_from_serial_fraction(double serial_fraction, int threads){
    if(threads <= 0){
        return 0.0;
    }
    double denominator = serial_fraction + ((1.0 - serial_fraction) / static_cast<double>(threads));
    if(denominator <= 0.0){
        return 0.0;
    }
    return 1.0 / denominator;
}

static double choose_projection_serial_fraction(const std::vector<ScalingResult>& measured_results){
    for(auto it = measured_results.rbegin(); it != measured_results.rend(); ++it){
        if(it->threads > 1){
            return it->serial_fraction;
        }
    }
    return 0.0;
}

//machine insight
static std::string trim_ws(const std::string& s){
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if(a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}
 
MachineInfo detect_machine_info(){
    MachineInfo m{};
 
    //get the cpu information
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    std::set<std::pair<std::string, std::string>> physical_cores;
    std::string cur_phys, cur_core;
 
    while(std::getline(cpuinfo, line)){
        if(line.empty()){
            if(!cur_phys.empty() && !cur_core.empty()){
                physical_cores.insert({cur_phys, cur_core});
            }
            cur_phys.clear();
            cur_core.clear();
            continue;
        }
        size_t colon = line.find(':');
        if(colon == std::string::npos) continue;
        std::string key = trim_ws(line.substr(0, colon));
        std::string val = trim_ws(line.substr(colon + 1));
 
        if(key == "processor")            m.logical_cores++;
        else if(key == "model name" && m.cpu_model.empty()) m.cpu_model = val;
        else if(key == "physical id")     cur_phys = val;
        else if(key == "core id")         cur_core = val;
    }
    //catch the last block if needed
    if(!cur_phys.empty() && !cur_core.empty()){
        physical_cores.insert({cur_phys, cur_core});
    }
    m.physical_cores = static_cast<int>(physical_cores.size());
 
    //fallbacks incase of errors
    if(m.logical_cores == 0){
        unsigned hc = std::thread::hardware_concurrency();
        m.logical_cores = (hc > 0) ? static_cast<int>(hc) : 1;
    }
    if(m.physical_cores == 0){
        m.physical_cores = m.logical_cores;//assume no SMT
    }
    if(m.cpu_model.empty()) m.cpu_model = "unknown CPU";
 
    //total ram
    std::ifstream meminfo("/proc/meminfo");
    while(std::getline(meminfo, line)){
        if(line.compare(0, 9, "MemTotal:") == 0){
            std::istringstream iss(line.substr(9));
            long kb = 0;
            iss >> kb;
            m.total_ram_mb = kb / 1024;
            break;
        }
    }
    struct utsname u{};
    if(uname(&u) == 0){
        m.kernel = std::string(u.sysname) + " " + u.release;
    }else{
        m.kernel = "unknown";
    }
 
    //compiler version
    #ifdef __VERSION__
        m.compiler = std::string("g++ ") + __VERSION__;
    #else
        m.compiler = "unknown";
    #endif
 
    return m;
}
 
void print_machine_info(const MachineInfo& m){
    std::cout << "Machine: " << m.cpu_model << "\n";
    std::cout << "Cores:   " << m.physical_cores << " physical / " << m.logical_cores  << " logical";
    if(m.logical_cores > m.physical_cores){
        std::cout << " (SMT enabled)";
    }
    std::cout << "\n";
    if(m.total_ram_mb > 0){
        std::cout << "Memory:  " << (m.total_ram_mb / 1024) << " GB\n";
    }
    std::cout << "Kernel:  " << m.kernel  << "\n";
    std::cout << "Compiler: " << m.compiler << "\n\n";
}
//verdict
OverallVerdict compute_overall_verdict(const std::vector<ScalingResult>& results, const MachineInfo& machine){
    OverallVerdict v{};
    v.confidence = "medium";
 
    if(results.size() < 2){
        v.label = "Insufficient data";
        v.explanation = "Need at least two thread counts to assess scaling.";
        v.confidence = "low";
        return v;
    }
 
    int max_threads = results.back().threads;
    double final_speedup = results.back().speedup;
    double final_efficiency = results.back().efficiency;
    double final_util = results.back().parallel_util;
    bool have_rusage = (final_util > 0.0);
 
    //karp flatt trajectory across multi-thread points
    std::vector<double> serial_fracs;
    for(const auto& r : results){
        if(r.threads > 1) serial_fracs.push_back(r.serial_fraction);
    }
    double avg_sf = 0.0, sf_growth = 0.0;
    if(!serial_fracs.empty()){
        for(double s : serial_fracs) avg_sf += s;
        avg_sf /= static_cast<double>(serial_fracs.size());
        if(serial_fracs.size() >= 2){
            sf_growth = serial_fracs.back() - serial_fracs.front();
        }
    }
 
    char buf[384];
 
    //false sharing/ memory contention
    bool always_subscalar = true;
    for(const auto& r : results){
        if(r.threads > 1 && r.speedup > 1.2){ always_subscalar = false; break; }
    }
    if(always_subscalar){
        v.label = "Cache contention or false sharing";
        std::snprintf(buf, sizeof(buf),
            "Speedup never exceeded 1.2x at any tested thread count (final: %.2fx at %d threads). "
            "This is the signature of shared-memory traffic dominating, typically false sharing "
            "or cache-line bouncing.",
            final_speedup, max_threads);
        v.explanation = buf;
        v.confidence  = "high";
        return v;
    }
 
    //constant high serial fraction
    if(avg_sf > 0.15 && std::fabs(sf_growth) < 0.06){
        std::snprintf(buf, sizeof(buf),
            "Karp-Flatt serial fraction averages %.0f%% and stays roughly constant across thread "
            "counts. Maximum attainable speedup is bounded by Amdahl's Law at ~%.1fx.",
            avg_sf * 100.0, 1.0 / avg_sf);
        v.label       = "Amdahl-bound serial fraction";
        v.explanation = buf;
        v.confidence  = "high";
        return v;
    }
 
    //growing serial fraction meaning synchronization or contention
    if(sf_growth > 0.05){
        std::snprintf(buf, sizeof(buf),
            "Karp-Flatt serial fraction grew from %.2f to %.2f across the sweep, the signature "
            "of thread-count-dependent overhead (lock contention, cache coherence traffic, or "
            "shared resource competition) rather than a fixed serial bottleneck.",
            serial_fracs.front(), serial_fracs.back());
        v.label       = "Synchronization or contention overhead";
        v.explanation = buf;
        v.confidence  = "high";
        return v;
    }
 
    //hardware aware
    if(machine.physical_cores > 0
       && max_threads > machine.physical_cores
       && have_rusage
       && final_util < 0.95){
        std::snprintf(buf, sizeof(buf),
            "Speedup degrades above %d threads, which equals this machine's physical core count. "
            "Threads beyond that share execution resources with their physical-core siblings (SMT), "
            "so each additional thread contributes only a fraction of a core's throughput.",
            machine.physical_cores);
        v.label = "SMT/hyperthread saturation";
        v.explanation = buf;
        v.confidence = "high";
        return v;
    }
 
    //healthy scaling
    double healthy_basis = (machine.physical_cores > 0)
        ? std::min((double)max_threads, (double)machine.physical_cores)
        : (double)max_threads;
    if(final_speedup > healthy_basis * 0.7){
        std::snprintf(buf, sizeof(buf),
            "Measured speedup at %d threads is %.2fx (%.0f%% efficiency). The program scales "
            "well across the tested range with no clear bottleneck.",
            max_threads, final_speedup, final_efficiency * 100.0);
        v.label = "Healthy scaling";
        v.explanation = buf;
        v.confidence = "high";
        return v;
    }
 
    //default
    std::snprintf(buf, sizeof(buf),
        "Scaling is positive but inefficient: %.2fx speedup at %d threads (%.0f%% efficiency). "
        "No single category fits cleanly; consider running a wider thread sweep or longer trials.",
        final_speedup, max_threads, final_efficiency * 100.0);
    v.label = "Diminishing returns";
    v.explanation = buf;
    v.confidence = "medium";
    return v;
}
 
void print_overall_verdict(const OverallVerdict& v){
    std::cout << "Overall verdict: " << v.label << " (confidence: " << v.confidence << ")\n";
    std::cout << v.explanation << "\n\n";
}

//build scaling results
std::vector<ScalingResult> build_scaling_results(const std::vector<int>& threads, const std::vector<std::vector<double>>& all_times){
    std::vector<ScalingResult> results;
    if(threads.empty() || all_times.empty() || threads.size() != all_times.size()){
        return results;
    }
    double baseline = compute_average(all_times[0]);

    for(size_t i = 0; i < threads.size(); ++i){
        ScalingResult r{};
        r.threads = threads[i];//store current thread count
        r.avg_time = compute_average(all_times[i]);
        r.min_time = compute_min(all_times[i]);
        r.max_time = compute_max(all_times[i]);
        r.stddev_time = compute_stddev(all_times[i], r.avg_time);
        r.ideal_speedup = static_cast<double>(threads[i]);

        if(r.avg_time <= 0.0){//dividing by 0 protection
            r.speedup = 0.0;
            r.efficiency = 0.0;
            r.serial_fraction = 0.0;
            r.max_theoretical_speedup = 0.0;
        }else{
            r.speedup = baseline / r.avg_time;
            r.efficiency = r.speedup / threads[i];

            if(threads[i] == 1){
                r.serial_fraction = 0.0;
                r.max_theoretical_speedup = 1.0;
            }else{
                r.serial_fraction = estimate_serial_fraction(r.speedup, r.threads);//estimate serial fraction from measured speedup

                if(r.serial_fraction > 0.0){
                    r.max_theoretical_speedup = 1.0 / r.serial_fraction;//theoretical max speedup
                }else{
                    r.max_theoretical_speedup = std::numeric_limits<double>::infinity();
                }
            }
        }

        results.push_back(r);
    }
    return results;
}

static RusageMetrics average_rusage(const std::vector<RusageMetrics>& trials){
    RusageMetrics avg{};
    if(trials.empty()) return avg;
 
    double inv_n = 1.0 / static_cast<double>(trials.size());
    double sum_user = 0, sum_sys = 0;
    long long sum_max_rss = 0, sum_vctx = 0, sum_ictx = 0;
    long long sum_min_pf = 0, sum_maj_pf = 0;
 
    for(const auto& m : trials){
        sum_user += m.user_time;
        sum_sys += m.system_time;
        sum_max_rss += m.max_rss;
        sum_vctx += m.voluntary_ctx_switches;
        sum_ictx += m.involuntary_ctx_switches;
        sum_min_pf += m.minor_page_faults;
        sum_maj_pf += m.major_page_faults;
    }
 
    avg.user_time = sum_user * inv_n;
    avg.system_time = sum_sys * inv_n;
    avg.max_rss = static_cast<long>(sum_max_rss * inv_n);
    avg.voluntary_ctx_switches = static_cast<long>(sum_vctx * inv_n);
    avg.involuntary_ctx_switches = static_cast<long>(sum_ictx * inv_n);
    avg.minor_page_faults = static_cast<long>(sum_min_pf * inv_n);
    avg.major_page_faults = static_cast<long>(sum_maj_pf * inv_n);
    return avg;
}
 
//fills in rusage and parallel util from scaling results.
void attach_rusage_to_results(std::vector<ScalingResult>& results, const std::vector<std::vector<RusageMetrics>>& all_rusage){
    if(all_rusage.size() != results.size()) return;
 
    for(size_t i = 0; i < results.size(); ++i){
        ScalingResult& r = results[i];
        r.rusage = average_rusage(all_rusage[i]);
        if(r.avg_time > 0.0 && r.threads > 0){
            r.parallel_util = r.rusage.user_time / (r.avg_time * static_cast<double>(r.threads));
        }
    }
}

std::vector<int> build_default_projection_threads(const std::vector<ScalingResult>& results){
    std::vector<int> projected_threads;
    if(results.empty()){
        return projected_threads;
    }

    int current = results.back().threads;
    if(current <= 0){
        return projected_threads;
    }

    for(int i = 0; i < 3; ++i){
        current *= 2;
        projected_threads.push_back(current);
    }

    return projected_threads;
}

std::vector<ProjectedScalingResult> build_projected_scaling_results(const std::vector<ScalingResult>& measured_results, const std::vector<int>& projected_threads){
    std::vector<ProjectedScalingResult> projected_results;
    if(measured_results.empty()){
        return projected_results;
    }

    double baseline_time = measured_results.front().avg_time;
    double serial_fraction = choose_projection_serial_fraction(measured_results);
    for(int threads : projected_threads){
        if(threads <= 0){
            continue;
        }
        ProjectedScalingResult p{};
        p.threads = threads;
        p.predicted_speedup = predict_speedup_from_serial_fraction(serial_fraction, threads);
        p.predicted_efficiency = (threads > 0) ? (p.predicted_speedup / static_cast<double>(threads)) : 0.0;
        p.predicted_time = (p.predicted_speedup > 0.0) ? (baseline_time / p.predicted_speedup) : 0.0;
        projected_results.push_back(p);
    }

    return projected_results;
}

void print_scaling_summary(const std::vector<ScalingResult>& results){//prints summary of all computed scaling metrics
    std::cout << "Scaling summary:\n";
    for(const auto& r : results){
        std::cout << "threads=" << r.threads
                  << " avg=" << std::fixed << std::setprecision(6) << r.avg_time << "s"
                  << " min=" << std::setprecision(6) << r.min_time << "s"
                  << " max=" << std::setprecision(6) << r.max_time << "s"
                  << " stddev=" << std::setprecision(6) << r.stddev_time << "s"
                  << " speedup=" << std::setprecision(3) << r.speedup
                  << " ideal=" << std::setprecision(3) << r.ideal_speedup
                  << " eff=" << std::setprecision(3) << r.efficiency;
        if(r.threads > 1){//only print serial fraction and max theoretical speedup for multithreaded runs
            std::cout << " serial_frac=" << std::setprecision(4) << r.serial_fraction;
            if(r.max_theoretical_speedup == std::numeric_limits<double>::infinity()){
                std::cout << " max_speedup=inf";
            }else{
                std::cout << " max_speedup=" << std::setprecision(3) << r.max_theoretical_speedup;
            }
        }

        if(r.rusage.user_time > 0.0 || r.rusage.system_time > 0.0){
            std::cout << "\n"
                      << "user=" << std::setprecision(4) << r.rusage.user_time << "s"
                      << " sys=" << std::setprecision(4) << r.rusage.system_time << "s"
                      << " util=" << std::setprecision(3) << r.parallel_util
                      << " maxRSS=" << r.rusage.max_rss << "KB"
                      << " ctx_v=" << r.rusage.voluntary_ctx_switches
                      << " ctx_i=" << r.rusage.involuntary_ctx_switches;
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

void print_projected_scaling_summary(
    const std::vector<ProjectedScalingResult>& projected_results, double serial_fraction_used){
    if(projected_results.empty()){
        return;
    }

    std::cout << "Projected scaling:\n";
    std::cout << "Using serial fraction estimate=" << std::fixed << std::setprecision(4)
              << serial_fraction_used << " from the highest measured thread count\n";

    for(const auto& p : projected_results){
        std::cout << "threads=" << p.threads
                  << " predicted_speedup=" << std::setprecision(3) << p.predicted_speedup
                  << " predicted_time=" << std::setprecision(6) << p.predicted_time << "s"
                  << " predicted_eff=" << std::setprecision(3) << p.predicted_efficiency
                  << "\n";
    }
    std::cout << "\n";
}

void print_diagnosis(const std::vector<ScalingResult>& results, const MachineInfo& machine){//simple rules based diagnosis of the scaling behavior
    std::cout << "Scaling diagnosis:\n";

    for(size_t i = 1; i < results.size(); ++i){
        const auto& prev = results[i - 1];
        const auto& curr = results[i];

        double speedup_gain = curr.speedup - prev.speedup;
        double relative_noise = (curr.avg_time > 0.0) ? (curr.stddev_time / curr.avg_time) : 0.0;

         //rusage signals if we get them
        bool have_rusage = (curr.rusage.user_time > 0.0 || curr.rusage.system_time > 0.0);
        double util_drop = have_rusage && prev.parallel_util > 0.0 ? (prev.parallel_util - curr.parallel_util) : 0.0;
        bool ictx_spike = have_rusage && curr.rusage.involuntary_ctx_switches >= 100
                        && prev.rusage.involuntary_ctx_switches > 0
                        && curr.rusage.involuntary_ctx_switches > 4 * prev.rusage.involuntary_ctx_switches;

        bool crossed_phys_core_boundary = (machine.physical_cores > 0) && (prev.threads <= machine.physical_cores) && (curr.threads >  machine.physical_cores);
        std::cout << prev.threads << " -> " << curr.threads << " threads: ";

        if(have_rusage && curr.parallel_util > 0.0 && curr.parallel_util < 0.70){
            std::cout << "Low parallel utilization ("
                      << std::fixed << std::setprecision(2) << curr.parallel_util
                      << "). Threads spent significant time stalled or waiting "
                         "(synchronization, I/O, or imbalance).\n";
        }else if(have_rusage && util_drop > 0.20 && crossed_phys_core_boundary){
            std::cout << "Parallel utilization dropped sharply ("
                      << std::fixed << std::setprecision(2) << prev.parallel_util
                      << " -> " << curr.parallel_util
                      << ") as thread count crossed the physical-core limit ("
                      << machine.physical_cores
                      << "). Strong SMT/hyperthread saturation signature.\n";
        }else if(have_rusage && util_drop > 0.20){
            std::cout << "Parallel utilization dropped sharply ("
                      << std::fixed << std::setprecision(2) << prev.parallel_util
                      << " -> " << curr.parallel_util
                      << "). Likely SMT/hyperthread saturation or contention.\n";
        }else if(ictx_spike){
            std::cout << "Involuntary context-switches spiked ("
                    << prev.rusage.involuntary_ctx_switches << " -> "
                    << curr.rusage.involuntary_ctx_switches << "). Possible oversubscription or scheduler contention.\n";

        }else if(curr.speedup > curr.threads * 1.1){
            std::cout << "Superlinear speedup detected. Possible caching effects or timing noise.\n";
        }else if(relative_noise > 0.05) {
            std::cout << "High timing variability detected. Results may be noisy.\n";
        }else if(curr.efficiency < 0.50) {
            std::cout << "Poor efficiency detected. Likely overhead, imbalance, or memory limits.\n";
        }else if(curr.serial_fraction > 0.10) {
            std::cout << "Noticeable serial fraction detected. Amdahl's Law may be limiting scaling.\n";
        }else if(speedup_gain < 0.30) {
            std::cout << "Small speedup gain. Scaling may be flattening.\n";
        }else{
            std::cout << "Scaling looks reasonable.\n";
        }
    }

    std::cout << "\n";
}

//writes the scaling metrics and the interpretation to a text file
void write_diagnosis_report(const std::vector<ScalingResult>& results, const std::vector<ProjectedScalingResult>& projected_results,
    double serial_fraction_used, const MachineInfo& machine, const OverallVerdict& overall, const std::string& filename){
    std::ofstream out(filename);

    if(!out){
        std::cerr << "Failed to open diagnosis report file: " << filename << "\n";
        return;
    }

    out << "ParaCheck Scaling Diagnosis Report\n\n";

    // Machine context block
    out << "Machine\n";
    out << "-------\n";
    out << "CPU:      " << machine.cpu_model << "\n";
    out << "Cores:    " << machine.physical_cores << " physical / "
                        << machine.logical_cores  << " logical";
    if(machine.logical_cores > machine.physical_cores) out << " (SMT enabled)";
    out << "\n";
    if(machine.total_ram_mb > 0){
        out << "Memory:   " << (machine.total_ram_mb / 1024) << " GB\n";
    }
    out << "Kernel:   " << machine.kernel  << "\n";
    out << "Compiler: " << machine.compiler << "\n";
 
    //tell user if oversubscribed
    if(!results.empty() && machine.physical_cores > 0
       && results.back().threads > machine.physical_cores){
        out << "\nNote: requested thread counts exceed this machine's physical core count ("
            << machine.physical_cores << "). Efficiency drop above "
            << machine.physical_cores << " threads is expected and may dominate the diagnosis.\n";
    }
    out << "\n";
 
    //overall verdict
    out << "Overall Verdict\n";
    out << "---------------\n";
    out << "Verdict:    " << overall.label << "\n";
    out << "Confidence: " << overall.confidence << "\n";
    out << overall.explanation << "\n\n";
    
    out << "Threads | Avg Time | Min Time | Max Time | Std Dev | Speedup | Ideal | Efficiency | Serial Fraction | Max Theoretical Speedup\n";
    out << "--------------------------------------------------------------------------------------------------------------------------------\n";
    for(const auto& r : results){
        out << std::setw(7) << r.threads << " | "
            << std::setw(8) << std::fixed << std::setprecision(6) << r.avg_time << " | "
            << std::setw(8) << r.min_time << " | "
            << std::setw(8) << r.max_time << " | "
            << std::setw(7) << r.stddev_time << " | "
            << std::setw(7) << std::setprecision(3) << r.speedup << " | "
            << std::setw(5) << r.ideal_speedup << " | "
            << std::setw(10) << r.efficiency << " | ";

        if(r.threads == 1){
            out << std::setw(15) << "-" << " | " << std::setw(23) << "-";
        }else{
            out << std::setw(15) << std::setprecision(4) << r.serial_fraction << " | ";

            if(r.max_theoretical_speedup == std::numeric_limits<double>::infinity()){
                out << std::setw(23) << "inf";
            }else{
                out << std::setw(23) << std::setprecision(3) << r.max_theoretical_speedup;
            }
        }
        out << "\n";
    }

    if(!projected_results.empty()){
       out << "\nProjected Results (Amdahl-based)\n";
        out << "Serial fraction used for projection: " << std::fixed << std::setprecision(4)
            << serial_fraction_used << "\n";
        out << "Threads | Predicted Speedup | Predicted Time | Predicted Efficiency\n";
        out << "----------------------------------------------------------------\n";
        for(const auto& p : projected_results){
            out << std::setw(7) << p.threads << " | "
                << std::setw(17) << std::fixed << std::setprecision(3) << p.predicted_speedup << " | "
                << std::setw(14) << std::setprecision(6) << p.predicted_time << " | "
                << std::setw(20) << std::setprecision(3) << p.predicted_efficiency << "\n";
        }
    }

    //only include rusage data if it has been collected
    bool any_rusage = false;
    for(const auto& r : results){
        if(r.rusage.user_time > 0.0 || r.rusage.system_time > 0.0){ any_rusage = true; break; }
    }
    if(any_rusage){
        out << "\nResource Usage (averaged per thread count)\n";
        out << "Threads | User Time | System Time | Util  | Max RSS (KB) | Vol Ctx | Invol Ctx\n";
        out << "------------------------------------------------------------------------------------\n";
        for(const auto& r : results){
            out << std::setw(7) << r.threads << " | "
                << std::setw(9) << std::fixed << std::setprecision(4) << r.rusage.user_time << " | "
                << std::setw(11) << std::setprecision(4) << r.rusage.system_time << " | "
                << std::setw(5) << std::setprecision(2) << r.parallel_util << " | "
                << std::setw(12) << r.rusage.max_rss << " | "
                << std::setw(7) << r.rusage.voluntary_ctx_switches << " | "
                << std::setw(9) << r.rusage.involuntary_ctx_switches << "\n";
        }
        out << "\nUtil = user_time / (wall_time * threads). 1.0 = every thread fully busy "
               "doing user work; lower = threads stalled, sleeping, or waiting.\n";
    }


    out << "\nInterpretation\n";
    out << "--------------\n";

    for(size_t i = 1; i < results.size(); ++i){
        const auto& prev = results[i - 1];
        const auto& curr = results[i];
        double speedup_gain = curr.speedup - prev.speedup;
        double relative_noise = (curr.avg_time > 0.0) ? (curr.stddev_time / curr.avg_time) : 0.0;
 
        bool have_rusage = (curr.rusage.user_time > 0.0 || curr.rusage.system_time > 0.0);
        double util_drop = have_rusage && prev.parallel_util > 0.0
                           ? (prev.parallel_util - curr.parallel_util)
                           : 0.0;
        bool ictx_spike = have_rusage && prev.rusage.involuntary_ctx_switches >= 100
                        && prev.rusage.involuntary_ctx_switches > 0
                        && curr.rusage.involuntary_ctx_switches >
                        4 * prev.rusage.involuntary_ctx_switches;

        bool crossed_phys_core_boundary = (machine.physical_cores > 0)
                                       && (prev.threads <= machine.physical_cores)
                                       && (curr.threads >  machine.physical_cores);
 
        out << prev.threads << " -> " << curr.threads << " threads: ";
 
        if(have_rusage && curr.parallel_util > 0.0 && curr.parallel_util < 0.70){
            out << "Low parallel utilization ("
                << std::fixed << std::setprecision(2) << curr.parallel_util
                << "). Threads spent significant time stalled or waiting; common causes are "
                   "synchronization, load imbalance, or I/O.\n";
        }else if(have_rusage && util_drop > 0.20 && crossed_phys_core_boundary){
            out << "Parallel utilization dropped sharply ("
                << std::fixed << std::setprecision(2) << prev.parallel_util
                << " -> " << curr.parallel_util
                << ") as thread count crossed the physical-core limit ("
                << machine.physical_cores
                << "). This is a strong SMT/hyperthread saturation signature.\n";
        }else if(have_rusage && util_drop > 0.20){
            out << "Parallel utilization dropped sharply ("
                << std::fixed << std::setprecision(2) << prev.parallel_util
                << " -> " << curr.parallel_util
                << "). This is the signature of SMT/hyperthread saturation or cache contention.\n";
        }else if(ictx_spike){
            out << "Involuntary context-switches spiked ("
                << prev.rusage.involuntary_ctx_switches << " -> "
                << curr.rusage.involuntary_ctx_switches
                << "). Likely oversubscription or scheduler contention.\n";
        }else if(curr.speedup > curr.threads * 1.1){
            out << "Superlinear speedup observed. This may be caused by cache effects or timing noise.\n";
        }else if(relative_noise > 0.08){
            out << "Timing variability is high, so benchmark results may be noisy.\n";
        }else if(curr.efficiency < 0.50) {
            out << "Poor efficiency detected. Possible causes include synchronization overhead, load imbalance, or memory bandwidth limits.\n";
        }else if(curr.serial_fraction > 0.10){
            out << "Estimated serial fraction is noticeable, so Amdahl's Law may be limiting additional speedup.\n";
        }else if(speedup_gain < 0.30){
            out << "Limited scaling improvement. Program may be reaching diminishing returns.\n";
        }else{
            out << "Scaling appears healthy.\n";
        }
    }
    if(!projected_results.empty()){
        const auto& last_projection = projected_results.back();
        out << "\nProjection note: future speedup was estimated with Amdahl's Law using the serial fraction from the highest measured thread count. "
            << "At " << last_projection.threads << " threads, projected speedup is "
            << std::fixed << std::setprecision(3) << last_projection.predicted_speedup
            << " with predicted efficiency " << std::setprecision(3) << last_projection.predicted_efficiency << ".\n";
    }
}