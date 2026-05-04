// amdahl_omp.c
// Demonstrates: AMDAHL-BOUND SERIAL FRACTION
//
// 30% of the work is in a serial loop that NO amount of parallelism can speed
// up. The remaining 70% is parallelized normally. Expected behavior:
//   - max speedup capped at ~1 / 0.30 = ~3.3x no matter how many threads
//   - Karp-Flatt e stays ~constant near 0.30 (vs growing in good_omp)
//   - parallel_util drops because the serial section runs single-threaded
//   - OMPCheck should fire "Noticeable serial fraction detected"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

int main(int argc, char** argv) {
    long n = (argc > 1) ? atol(argv[1]) : 80000000L;

    volatile double sink = 0.0;

    // SERIAL section: 30% of total work, single-threaded by construction
    long serial_n = (n * 3) / 10;
    for (long i = 0; i < serial_n; i++) {
        double x = (double)i * 1e-9;
        sink += sin(x) * cos(x) + sqrt(x + 1.0);
    }

    // PARALLEL section: remaining 70% of work
    #pragma omp parallel for reduction(+:sink) schedule(static)
    for (long i = serial_n; i < n; i++) {
        double x = (double)i * 1e-9;
        sink += sin(x) * cos(x) + sqrt(x + 1.0);
    }

    if (sink == 1234567.0) printf("magic\n");
    return 0;
}