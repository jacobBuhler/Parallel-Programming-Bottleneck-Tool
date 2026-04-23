//diagnosis.cpp

#include "diagnosis.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <limits>
#include <algorithm>
#include <cmath>

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
        std::cout << "\n";
    }
    std::cout << "\n";
}

void print_projected_scaling_summary(
    const std::vector<ProjectedScalingResult>& projected_results,
    double serial_fraction_used
){
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

void print_diagnosis(const std::vector<ScalingResult>& results){//simple rules based diagnosis of the scaling behavior
    std::cout << "Scaling diagnosis:\n";

    for(size_t i = 1; i < results.size(); ++i){
        const auto& prev = results[i - 1];
        const auto& curr = results[i];

        double speedup_gain = curr.speedup - prev.speedup;
        double relative_noise = (curr.avg_time > 0.0) ? (curr.stddev_time / curr.avg_time) : 0.0;
        std::cout << prev.threads << " -> " << curr.threads << " threads: ";

        if(curr.speedup > curr.threads * 1.1){
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
void write_diagnosis_report(const std::vector<ScalingResult>& results, const std::vector<ProjectedScalingResult>& projected_results, double serial_fraction_used, const std::string& filename){
    std::ofstream out(filename);

    if(!out){
        std::cerr << "Failed to open diagnosis report file: " << filename << "\n";
        return;
    }

    out << "OMPCheck Scaling Diagnosis Report\n\n";
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

    out << "\nInterpretation\n";
    out << "--------------\n";

    for(size_t i = 1; i < results.size(); ++i){
        const auto& prev = results[i - 1];
        const auto& curr = results[i];
        double speedup_gain = curr.speedup - prev.speedup;
        double relative_noise = (curr.avg_time > 0.0) ? (curr.stddev_time / curr.avg_time) : 0.0;
        out << prev.threads << " -> " << curr.threads << " threads: ";

        if(curr.speedup > curr.threads * 1.1){
            out << "Superlinear speedup observed. This may be caused by cache effects or timing noise.\n";
        }else if(relative_noise > 0.05){
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
