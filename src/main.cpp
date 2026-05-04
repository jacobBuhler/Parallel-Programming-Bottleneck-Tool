//main para checker file
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <fstream>

#include <unistd.h> //fork & execvp
#include <sys/wait.h> //waitpid, WIFEXITED & WEXITSTATUS
#include <sys/resource.h> //wait4 & struct rusage

#include "diagnosis.h"

enum class Paradigm { OpenMP, MPI, Pthreads};

//convert a kernel struct rusage into our project's RusageMetrics.
static RusageMetrics rusage_to_metrics(const struct rusage& ru){
    RusageMetrics m{};
    m.user_time = (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec * 1e-6;
    m.system_time = (double)ru.ru_stime.tv_sec + (double)ru.ru_stime.tv_usec * 1e-6;
    m.max_rss = ru.ru_maxrss;
    m.voluntary_ctx_switches = ru.ru_nvcsw;
    m.involuntary_ctx_switches = ru.ru_nivcsw;
    m.minor_page_faults = ru.ru_minflt;
    m.major_page_faults = ru.ru_majflt;
    return m;
}

static Paradigm parse_paradigm(const std::string& s){
    if(s == "openmp") return Paradigm::OpenMP;
    if(s == "mpi") return Paradigm::MPI;
    if(s == "pthreads") return Paradigm::Pthreads;
    std::cerr << "Unknown paradigm: " << s << "\n";
    std::exit(2);
}

static void usage(const char* prog) { //print usage and exit w code 2
    std::cerr << "Usage:\n" << "  " << prog << " --threads 1,2,4,8 [--runs N] [--csv out.csv] [--plot] [--report out.txt] [--paradigm openmp|mpi|pthreads] -- <command> [args...]\n";
    std::exit(2);
}

//returns the current time using monotonic clock
static double now_seconds_monotonic(){
    timespec ts{};
    
    //fills ts with current monotonic time, if fails print an error message
    if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0){
        std::perror("clock_gettime");
        std::exit(1);
  }
    return(double)ts.tv_sec + (double)ts.tv_nsec * 1e-9; //convert seconds and nanoseconds in a double val in seconds
}

//parse thread list & returns a vector<int> of thread counts
static std::vector<int> parse_threads(const std::string& s){
    std::vector<int> out;
    std::stringstream ss(s);//allows the split w commas
    std::string token;

    while(std::getline(ss, token, ',')){//split string by commas
        if(token.empty()) continue;
        out.push_back(std::stoi(token));//token to int
    }

    if(out.empty()){//if no valid thread, exit
        std::cerr << "No valid thread counts.\n";
        std::exit(2);
    }

    std::sort(out.begin(), out.end());//sort thread count
    return out;
}

//runs the target program using fork + execvp functions
//fork creates a new process and wait4 allows us tto captue rusage in same call
static int run_child(char* const child_argv[], RusageMetrics* out_ru = nullptr){
    pid_t pid = fork();
    if(pid < 0){
        std::perror("fork");
        return -1;
    }
 
    if(pid == 0){
        execvp(child_argv[0], child_argv);
        std::perror("execvp");
        _exit(127); //exit child
    }
 
    int status = 0;
    struct rusage ru{};
 
    if(wait4(pid, &status, 0, &ru) < 0){
        std::perror("wait4");
        return -1;
    }
 
    if(out_ru) *out_ru = rusage_to_metrics(ru);
    return status;
}

//checks if child process exited normally with code 0
static bool status_ok(int status){
    return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
}

//removes .csv from filename
static std::string strip_csv_extension(const std::string& path){
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".csv"){
        return path.substr(0, path.size() - 4);
    }
    return path;
}
//runs the python plotting script to generate the performance graphs
static void run_plot_script(const std::string& csv_path, const std::string& title){
    std::string prefix = strip_csv_extension(csv_path);
    std::string command =
        "python3 scripts/plot_results.py \"" +
        csv_path + "\" \"" +
        prefix + ".png\" \"" +
        title + "\"";

    int rc = std::system(command.c_str());
    if(rc != 0){
        std::cerr << "Warning: plotting script failed.\n";
    }
}

static int run_with_paradigm(Paradigm paradigm, int thread_count, char* const user_argv[], int user_argc, RusageMetrics* out_ru = nullptr){
    switch(paradigm){
    case Paradigm::OpenMP:
        setenv("OMP_NUM_THREADS", std::to_string(thread_count).c_str(), 1);//set OpenMP thread count
        return run_child(user_argv, out_ru);
 
    case Paradigm::MPI: {
        std::string np_str = std::to_string(thread_count);
        std::vector<const char*> v;
        v.push_back("mpirun");
        v.push_back("--oversubscribe");
        v.push_back("-np");
        v.push_back(np_str.c_str());
        for(int k = 0; k < user_argc; ++k) v.push_back(user_argv[k]);
        v.push_back(nullptr);
        return run_child(const_cast<char* const*>(v.data()), out_ru);
    }
    case Paradigm::Pthreads: {
        std::string n_str = std::to_string(thread_count);
        std::vector<const char*> v;
        v.push_back(user_argv[0]);
        v.push_back(n_str.c_str());
        for(int k = 1; k < user_argc; ++k) v.push_back(user_argv[k]);
        v.push_back(nullptr);
        return run_child(const_cast<char* const*>(v.data()), out_ru);
    }
    }
    return -1;//unreachebale
}

//main func
int main(int argc, char** argv){
    if(argc < 2){ //arg checker
        usage(argv[0]);
    }
    std::string threads_arg;
    int runs = 5;

    std::string csv_path;
    bool csv_enabled = false;

    bool plot_enabled = false;
    std::string report_path = "output/diagnosis.txt";
    Paradigm paradigm = Paradigm::OpenMP;

    int i = 1;
    for(; i < argc; ++i){
        std::string a = argv[i];
        if(a == "--"){
            ++i;
            break;
        }
        if(a == "--threads"){
            if(i + 1 >= argc) usage(argv[0]);
            threads_arg = argv[++i];
        }else if(a == "--runs"){
            if(i + 1 >= argc) usage(argv[0]);
            runs = std::stoi(argv[++i]);
            if(runs <= 0){
                std::cerr << "--runs must be >= 1\n";
                std::exit(2);
            }
        }else if(a == "--csv"){//handle csv
            if(i + 1 >= argc) usage(argv[0]);
                csv_path = argv[++i];
                csv_enabled = true;
        }else if(a == "--plot") {
            plot_enabled = true;
        }else if (a == "--report"){
            if (i + 1 >= argc) usage(argv[0]);
            report_path = argv[++i];
        }else if(a == "--paradigm"){
            if(i + 1 >= argc) usage(argv[0]);
            paradigm = parse_paradigm(argv[++i]);
        }else{
            usage(argv[0]);//unknown flag
        }
    }
  //must have thread list and command
    if(threads_arg.empty() || i >= argc){
        usage(argv[0]);
    }

    if(plot_enabled && !csv_enabled){
        std::cerr << "--plot requires --csv <file>\n";
        return 1;
    }

    //child argv now points to target program and its arguments
    char** child_argv = &argv[i];
    int child_argc = argc - i;
    auto threads = parse_threads(threads_arg);//convert thread list to vector<int>

    std::vector<std::vector<double>> all_times;
    std::vector<std::vector<RusageMetrics>> all_rusage;

    std::ofstream csv;
    if(csv_enabled){//open csv if req
        csv.open(csv_path);
        if(!csv){
            std::cerr << "Failed to open CSV file: " << csv_path << "\n";
            return 1;
        }
    }

    MachineInfo machine = detect_machine_info();
    print_machine_info(machine);

    std::cout << "Command: ";
    for(int k = i; k < argc; ++k){
        std::cout << argv[k] << (k + 1 < argc ? " " : "");
    }
    std::cout << "\n";
    std::cout << "Runs per thread count: " << runs << "\n\n";

    if(csv_enabled){
        csv << "# runs=" << runs << "\n";
        csv << "# command=";
        for(int k = i; k < argc; ++k){
            csv << argv[k] << (k + 1 < argc ? " " : "");
        }
        csv << "\n";
        csv << "threads,trial,time,user_time,system_time,max_rss,vol_ctx,invol_ctx\n";
    }
    
    for(size_t idx = 0; idx < threads.size(); ++idx){//loop over each thread count
        int t = threads[idx];
        setenv("OMP_NUM_THREADS", std::to_string(t).c_str(), 1);//set OpenMP thread count
        std::vector<double> thread_times;
        std::vector<RusageMetrics> thread_rusage;
        
        for(int r = 0; r < runs; ++r){
            RusageMetrics ru{};
            double t0 = now_seconds_monotonic();//timer start
            int status = run_with_paradigm(paradigm, t, child_argv, child_argc, &ru);//run target program
            double t1 = now_seconds_monotonic();//end timer

            if(!status_ok(status)){//check for failure
                std::cerr << "Run failed for threads=" << t << " (trial " << (r+1) << ")\n";
                return 1;
            }

            double elapsed = t1 - t0;
            thread_times.push_back(elapsed);
            thread_rusage.push_back(ru);

            if(csv_enabled){
                csv << t << "," << (r+1)
                    << "," << std::fixed << std::setprecision(9) << elapsed
                    << "," << std::setprecision(6) << ru.user_time
                    << "," << std::setprecision(6) << ru.system_time
                    << "," << ru.max_rss
                    << "," << ru.voluntary_ctx_switches
                    << "," << ru.involuntary_ctx_switches
                    << "\n";
            }
        }
        all_times.push_back(thread_times);
        all_rusage.push_back(thread_rusage);
        
    }

    //compute scaling metrics and diagnostics
    auto results = build_scaling_results(threads, all_times);
    attach_rusage_to_results(results, all_rusage);
    auto projected_threads = build_default_projection_threads(results);
    auto projected_results = build_projected_scaling_results(results, projected_threads);
    double projection_serial_fraction = 0.0;
    if(results.size() > 1){
        projection_serial_fraction = results.back().serial_fraction;
    }

    OverallVerdict overall = compute_overall_verdict(results, machine);
    //print resilts to terminal
    print_scaling_summary(results);
    print_projected_scaling_summary(projected_results, projection_serial_fraction);
    print_overall_verdict(overall);
    print_diagnosis(results, machine);
    

    //write report file
    write_diagnosis_report(results, projected_results, projection_serial_fraction, machine, overall, report_path);

    if(csv_enabled){//close file
    csv.flush();
    csv.close();
    }

    if(plot_enabled){
        run_plot_script(csv_path, "ParaCheck Scaling Results");
        std::cout << "\nPlots generated from: " << csv_path << "\n";
    }

  return 0;
}
