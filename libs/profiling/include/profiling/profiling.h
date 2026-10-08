#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PROFILING_STOPWATCH_COUNT 8

#ifndef PROFILING_ENABLED
#define PROFILING_ENABLED 1
#endif

/* A 32-bit sum overflows after about 71 minutes of accumulated microseconds. */
typedef struct {
    const char *name;
    uint32_t count;
    float mean_us;
    uint32_t max_us;
} profiling_result_t;

void profiling_stopwatch_init(uint32_t id, const char *name);
#if PROFILING_ENABLED
void profiling_start(uint32_t id);
void profiling_stop(uint32_t id);
#else
#define profiling_start(id) ((void)0)
#define profiling_stop(id) ((void)0)
#endif
bool profiling_get(uint32_t id, profiling_result_t *result);
void profiling_reset(void);
