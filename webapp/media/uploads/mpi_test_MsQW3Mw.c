// MPI test program — parallel array sum
// Each process sums a chunk of a large array, then MPI_Reduce combines them.
// Compile: mpicc -O2 mpi_test.c -lm -o mpi_test
// Run:     mpirun -np 4 ./mpi_test

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

#define ARRAY_SIZE 10000000  // 10 million elements

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // divide the work evenly across processes
    int chunk = ARRAY_SIZE / size;
    int start = rank * chunk;
    int end   = (rank == size - 1) ? ARRAY_SIZE : start + chunk;

    // each process computes its local sum
    double local_sum = 0.0;
    for (int i = start; i < end; i++) {
        local_sum += sqrt((double)i) * sin((double)i * 0.0001);
    }

    // reduce all local sums to rank 0
    double global_sum = 0.0;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Global sum: %.6f  (processes: %d)\n", global_sum, size);
    }

    MPI_Finalize();
    return 0;
}
