//bad_omp.c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

int main(int argc, char** argv) {
    long n = (argc > 1) ? atol(argv[1]) : 80000000L;

    volatile double sink = 0.0;

    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) {
        double x = (double)i * 1e-9;
        double val = sin(x) * cos(x) + sqrt(x + 1.0);

        //bad because it forces threads to serialize constantly
        #pragma omp critical
        {
            sink += val;
        }
    }

    if (sink == 1234567.0) printf("magic\n");
    return 0;
}
