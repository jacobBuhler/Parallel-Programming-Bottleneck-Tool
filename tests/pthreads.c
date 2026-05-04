#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

#define ARRAY_SIZE 10000000

int    num_threads;
double partial_sums[256];

typedef struct {
    int thread_id;
    int start;
    int end;
} ThreadArgs;

void* compute_sum(void* arg) {
    ThreadArgs* a = (ThreadArgs*)arg;
    double sum = 0.0;
    for (int i = a->start; i < a->end; i++) {
        sum += sqrt((double)i) * sin((double)i * 0.0001);
    }
    partial_sums[a->thread_id] = sum;
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <num_threads>\n", argv[0]);
        return 1;
    }

    num_threads = atoi(argv[1]);
    if (num_threads <= 0 || num_threads > 256) {
        fprintf(stderr, "Thread count must be between 1 and 256.\n");
        return 1;
    }

    pthread_t  threads[256];
    ThreadArgs args[256];
    int chunk = ARRAY_SIZE / num_threads;

    for (int t = 0; t < num_threads; t++) {
        args[t].thread_id = t;
        args[t].start     = t * chunk;
        args[t].end       = (t == num_threads - 1) ? ARRAY_SIZE : args[t].start + chunk;
        pthread_create(&threads[t], NULL, compute_sum, &args[t]);
    }

    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }

    double total = 0.0;
    for (int t = 0; t < num_threads; t++) {
        total += partial_sums[t];
    }

    printf("Total sum: %.6f  (threads: %d)\n", total, num_threads);
    return 0;
}
