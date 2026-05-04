#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile double g_sink = 0.0;

typedef struct {
    long start;
    long end;
} ThreadArgs;

static void* worker(void* arg) {
    ThreadArgs* t = (ThreadArgs*)arg;
    for (long i = t->start; i < t->end; i++) {
        double x = (double)i * 1e-9;
        double r = sin(x) * cos(x) + sqrt(x + 1.0);

        pthread_mutex_lock(&g_lock);
        g_sink += r;
        pthread_mutex_unlock(&g_lock);
    }
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <num_threads> [n]\n", argv[0]);
        return 1;
    }
    int nthreads = atoi(argv[1]);
    if (nthreads < 1) nthreads = 1;
    long n = (argc > 2) ? atol(argv[2]) : 20000000L;

    pthread_t* threads = (pthread_t*)malloc(nthreads * sizeof(pthread_t));
    ThreadArgs* args   = (ThreadArgs*)malloc(nthreads * sizeof(ThreadArgs));

    for (int i = 0; i < nthreads; i++) {
        args[i].start = (n * (long)i) / nthreads;
        args[i].end   = (n * (long)(i + 1)) / nthreads;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    if (g_sink == 1234567.0) printf("magic\n");

    free(threads);
    free(args);
    return 0;
}