#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    long n = (argc > 1) ? atol(argv[1]) : 80000000L;

    //partition the loop range across ranks
    long start = (n * (long)rank) / size;
    long end   = (n * (long)(rank + 1)) / size;

    volatile double local_sum = 0.0;
    for (long i = start; i < end; i++) {
        double x = (double)i * 1e-9;
        local_sum += sin(x) * cos(x) + sqrt(x + 1.0);
    }

    double global_sum = 0.0;
    MPI_Reduce((double*)&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0 && global_sum == 1234567.0) printf("magic\n");

    MPI_Finalize();
    return 0;
}