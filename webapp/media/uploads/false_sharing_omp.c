// false_sharing_omp.c
// Demonstrates: FALSE SHARING / CACHE-LINE PING-PONG
//
// Each thread updates its own counter, but the counters are stored in adjacent
// elements of a small array. A typical cache line is 64 bytes, so 8 doubles
// share a single line; every write from one thread invalidates the line for
// every other thread, causing the cache line to bounce between cores.
//
// At the source level this looks like trivially parallel work, but at runtime
// it scales terribly. Expected behavior:
//   - speedup stalls or even regresses past 1-2 threads
//   - parallel_util collapses
//   - OMPCheck should fire "Poor efficiency" or "Low parallel utilization"
//
// This is the classic "looks fine, runs slow" trap.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define MAX_THREADS 64

int main(int argc, char** argv) {
    long n = (argc > 1) ? atol(argv[1]) : 40000000L;

    // Adjacent doubles share cache lines: classic false-sharing setup.
    static volatile double counters[MAX_THREADS] = {0};

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int nt  = omp_get_num_threads();
        long start = tid * (n / nt);
        long end   = (tid == nt - 1) ? n : start + (n / nt);

        for (long i = start; i < end; i++) {
            double x = (double)i * 1e-9;
            // every iteration writes back to a counter that shares a cache
            // line with the neighbouring threads' counters
            counters[tid] += sin(x) * cos(x) + sqrt(x + 1.0);
        }
    }

    double sum = 0.0;
    for (int i = 0; i < MAX_THREADS; i++) sum += counters[i];
    if (sum == 1234567.0) printf("magic\n");
    return 0;
}