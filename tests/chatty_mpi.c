//communication overhead test
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    long n = (argc > 1) ? atol(argv[1]) : 200000L;

    volatile double sink = 0.0;
    for (long i = 0; i < n; i++) {
        double local = sin((double)(i + rank) * 1e-6);
        double global = 0.0;

        MPI_Allreduce(&local, &global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        sink += global;
    }

    if (rank == 0 && sink == 1234567.0) printf("magic\n");

    MPI_Finalize();
    return 0;
}