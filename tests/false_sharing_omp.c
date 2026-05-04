#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define MAX_THREADS 64

int main(int argc, char** argv) {
    long n = (argc > 1) ? atol(argv[1]) : 40000000L;
    static volatile double counters[MAX_THREADS] = {0};

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int nt  = omp_get_num_threads();
        long start = tid * (n / nt);
        long end   = (tid == nt - 1) ? n : start + (n / nt);

        for (long i = start; i < end; i++) {
            double x = (double)i * 1e-9;
            counters[tid] += sin(x) * cos(x) + sqrt(x + 1.0);
        }
    }

    double sum = 0.0;
    for (int i = 0; i < MAX_THREADS; i++) sum += counters[i];
    if (sum == 1234567.0) printf("magic\n");
    return 0;
}