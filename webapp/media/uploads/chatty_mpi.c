// chatty_mpi.c
// Demonstrates: MPI COMMUNICATION OVERHEAD (the MPI equivalent of a critical
// section bottleneck)
//
// Every iteration involves an MPI_Allreduce across all ranks. Each iteration's
// actual compute is tiny, so synchronization and message-passing overhead
// dominate. Adding ranks makes the program SLOWER.
//
// Expected behavior with ParaCheck --paradigm mpi:
//   - speedup stays near or below 1.0
//   - Karp-Flatt e grows or saturates at 1.0
//   - parallel_util collapses (each rank waits for the slowest in the barrier)
//   - Overall Verdict: "Synchronization or contention overhead"
//                       OR "Cache contention or false sharing"
//                       (depending on whether speedup ever breaks 1.2x)
//
// This is the MPI mirror of bad_critical_omp.c.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // smaller default n than good_mpi: each iteration is now expensive
    // because of the all-rank synchronization.
    long n = (argc > 1) ? atol(argv[1]) : 200000L;

    volatile double sink = 0.0;
    for (long i = 0; i < n; i++) {
        double local = sin((double)(i + rank) * 1e-6);
        double global = 0.0;
        // Every iteration: every rank must reach this line and exchange data.
        // O(log size) communication per iteration -> kills scaling.
        MPI_Allreduce(&local, &global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        sink += global;
    }

    if (rank == 0 && sink == 1234567.0) printf("magic\n");

    MPI_Finalize();
    return 0;
}