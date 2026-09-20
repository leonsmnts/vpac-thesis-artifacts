#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>

typedef double (*perf_timer_func_t)(void);

typedef struct perf_timer perf_timer_t;

perf_timer_t *perf_timer_create(perf_timer_func_t func, int capacity, const char *filename);
void perf_timer_measure(perf_timer_t *t);
void perf_timer_save(perf_timer_t *t);
void perf_timer_free(perf_timer_t *t);

void perf_timer_stats(perf_timer_t *t, const char *label);

#endif /* TIMING_H */
