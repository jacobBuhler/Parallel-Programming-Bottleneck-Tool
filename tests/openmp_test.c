#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define ARRAY_SIZE 10000000

int main(int argc, char** argv) {
    double total = 0.0;

    #pragma omp parallel for reduction(+:total)
    for (int i = 0; i < ARRAY_SIZE; i++) {
        total += sqrt((double)i) * sin((double)i * 0.0001);
    }

    printf("Total sum: %.6f  (threads: %d)\n", total, omp_get_max_threads());
    return 0;
}
