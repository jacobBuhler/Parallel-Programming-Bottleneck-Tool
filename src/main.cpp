//main omp checker file

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

#include <unistd.h> //fork & execvp
#include <sys/wait.h> //waitpid, WIFEXITED & WEXITSTATUS

static void usage(const char* prog) { //print usage and exit w code 2
    std::cerr << "Usage:\n" << "  " << prog << " --threads 1,2,4,8 -- <command> [args...]\n";
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
//fork creates a new process
static int run_child(char* const child_argv[]){
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
    waitpid(pid, &status, 0);
    return status;
}

//checks if child process exited normally with code 0
static bool status_ok(int status){
    return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
}

//main func
int main(int argc, char** argv){
    if(argc < 2){ //arg checker
        usage(argv[0]);
    }
    std::string threads_arg;
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
    } else{
        usage(argv[0]);//unknown flag
      }
    }
  //must have thread list and command
    if(threads_arg.empty() || i >= argc){
        usage(argv[0]);
    }

    //child argv now points to target program and its arguments
    char** child_argv = &argv[i];
    auto threads = parse_threads(threads_arg);//convert thread list to vector<int>

    std::cout << "Command: ";
    for(int k = i; k < argc; ++k){
        std::cout << argv[k] << (k + 1 < argc ? " " : "");
        std::cout << "\n\n";
    }

    double baseline_time = 0.0;//time for first thread count

    for(size_t idx = 0; idx < threads.size(); ++idx){//loop over each thread count
        int t = threads[idx];

        setenv("OMP_NUM_THREADS", std::to_string(t).c_str(), 1);//set OpenMP thread count for the program we run

        double t0 = now_seconds_monotonic();//timer start
        int status = run_child(child_argv);//run target program
        double t1 = now_seconds_monotonic();//end timer

    if(!status_ok(status)){//check for failure
        std::cerr << "Run failed for threads=" << t << "\n";
        return 1;
    }

    double elapsed = t1 - t0;

    if(idx == 0){
        baseline_time = elapsed;
    }

    double speedup = baseline_time / elapsed;//compute speedup

    std::cout << "threads=" << t << " time=" << std::fixed << std::setprecision(6) << elapsed << "s" << " speedup=" << std::setprecision(3) << speedup << "\n";
  }

  return 0;
}
