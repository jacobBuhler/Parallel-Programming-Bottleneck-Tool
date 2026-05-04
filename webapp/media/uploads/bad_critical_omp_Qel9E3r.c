// bad_critical_omp.c
// Demonstrates: SYNCHRONIZATION OVERHEAD / SERIALIZATION
//
// Every iteration's accumulation is wrapped in #pragma omp critical, so only
// one thread at a time can do the increment. The work itself is parallelized,
// but the lock serializes it. Expected behavior:
//   - speedup stays near or below 1.0 even at 8 threads
//   - parallel_util is very low (threads spend most of their time waiting on
//     the lock)
//   - OMPCheck should fire "Low parallel utilization" diagnosis branch
//
// This is a worst-case demo of why naive critical sections kill scaling.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

int main(int argc, char** argv) {
    long n = (argc > 1) ? atol(argv[1]) : 20000000L;

    volatile double sink = 0.0;

    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        double x = (double)i * 1e-9;
        double r = sin(x) * cos(x) + sqrt(x + 1.0);

        // every thread funnels through this lock — serializes the program
        #pragma omp critical
        {
            sink += r;
        }
    }
    if (sink == 1234567.0) printf("magic\n");
    return 0;
}