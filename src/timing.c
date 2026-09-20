#define _GNU_SOURCE  // Enables CLOCK_MONOTONIC_RAW and POSIX functions (clock_gettime, strdup)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "timing.h"

struct perf_timer {
    perf_timer_func_t func;
    double *deltas;
    int capacity;
    int count;
    char *filename;
};

static volatile double g_perf_timer_sink;  // Prevent compiler from optimizing away the benchmarked function

perf_timer_t *perf_timer_create(perf_timer_func_t func, int capacity, const char *filename) {
    perf_timer_t *t = malloc(sizeof(perf_timer_t));
    if (!t) {
        perror("perf_timer_create malloc perf_timer_t");
        return NULL;
    }
    
    t->func = func;
    t->capacity = capacity;
    t->count = 0;
    t->filename = strdup(filename);
    t->deltas = malloc(capacity * sizeof(double));
    
    if (!t->filename || !t->deltas) {
        perror("perf_timer_create alloc");
        perf_timer_free(t);
        return NULL;
    }

    // Pre-fault the memory!
    // Loop through deltas array and write a '0' to every single slot.
    // This forces Linux to allocate all the physical RAM before the benchmark starts.
    // This prevents page faults during the benchmark, which would skew timing results.
    for (int i = 0; i < t->capacity; i++) {
        t->deltas[i] = 0.0; 
    }
    
    return t;
}

void perf_timer_measure(perf_timer_t *t) {
    if (!t || t->count >= t->capacity) return;
    
    struct timespec before, after;
    clock_gettime(CLOCK_MONOTONIC_RAW, &before);
    
    double result = t->func();
    
    clock_gettime(CLOCK_MONOTONIC_RAW, &after);
    
    g_perf_timer_sink += result;  // Prevent compiler from optimizing away the benchmarked function

    double dt_us = (after.tv_sec - before.tv_sec)*1e6 + 
                   (after.tv_nsec - before.tv_nsec)/1e3;
    
    t->deltas[t->count++] = dt_us;
}

void perf_timer_save(perf_timer_t *t) {
    if (!t) return;
    
    FILE *f = fopen(t->filename, "w");
    if (!f) {
        perror("perf_timer_save fopen");
        return;
    }
    
    fprintf(f, "iteration,delta_us\n");
    for (int i = 0; i < t->count; i++) {
        fprintf(f, "%d,%.3f\n", i, t->deltas[i]);
    }
    fclose(f);
    
    printf("Saved %s (%d samples)\n", t->filename, t->count);
}

void perf_timer_free(perf_timer_t *t) {
    if (t) {
        free(t->deltas);
        free(t->filename);
        free(t);
    }
}

void perf_timer_stats(perf_timer_t *t, const char *label) {
    if (!t || t->count == 0) {
        printf("No samples for %s\n", label);
        return;
    }
    
    double minv = t->deltas[0], maxv = t->deltas[0], sum = 0.0;
    for (int i = 0; i < t->count; i++) {
        double v = t->deltas[i];
        if (v < minv) minv = v;
        if (v > maxv) maxv = v;
        sum += v;
    }
    double avg = sum / t->count;
    
    printf("%s stats over %d samples: "
           "min=%.2f μs, max=%.2f μs, avg=%.2f μs\n",
           label, t->count, minv, maxv, avg);
}

