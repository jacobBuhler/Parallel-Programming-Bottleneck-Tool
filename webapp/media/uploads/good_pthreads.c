#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

typedef struct {
    long start;
    long end;
    double partial_sum;
} ThreadArgs;

static void* worker(void* arg) {
    ThreadArgs* t = (ThreadArgs*)arg;
    double sum = 0.0;
    for (long i = t->start; i < t->end; i++) {
        double x = (double)i * 1e-9;
        sum += sin(x) * cos(x) + sqrt(x + 1.0);
    }
    t->partial_sum = sum;
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <num_threads> [n]\n", argv[0]);
        return 1;
    }
    int nthreads = atoi(argv[1]);
    if (nthreads < 1) nthreads = 1;
    long n = (argc > 2) ? atol(argv[2]) : 80000000L;

    pthread_t* threads = (pthread_t*)malloc(nthreads * sizeof(pthread_t));
    ThreadArgs* args   = (ThreadArgs*)malloc(nthreads * sizeof(ThreadArgs));

    for (int i = 0; i < nthreads; i++) {
        args[i].start = (n * (long)i) / nthreads;
        args[i].end   = (n * (long)(i + 1)) / nthreads;
        args[i].partial_sum = 0.0;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    volatile double total = 0.0;
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
        total += args[i].partial_sum;
    }

    if (total == 1234567.0) printf("magic\n");

    free(threads);
    free(args);
    return 0;
}