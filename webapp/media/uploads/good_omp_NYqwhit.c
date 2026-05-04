//good_omp.c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

int main(int argc, char** argv) {
    long n = (argc > 1) ? atol(argv[1]) : 200000000L;

    volatile double sink = 0.0;

    #pragma omp parallel for reduction(+:sink) schedule(static)
    for (long i = 0; i < n; i++) {
        double x = (double)i * 1e-9;
        sink += sin(x) * cos(x) + sqrt(x + 1.0);
    }
    if (sink == 1234567.0) printf("magic\n");
    return 0;
}
