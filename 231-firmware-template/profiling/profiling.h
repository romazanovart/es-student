#ifndef PROFILING_H
#define PROFILING_H

#include <stdbool.h>
#include <stdint.h>

#define PROFILING_STOPWATCH_COUNT 8

/* A 32-bit sum overflows after about 71 minutes of accumulated microseconds. */
typedef struct {
    const char *name;
    uint32_t count;
    float mean_us;
    uint32_t max_us;
} profiling_result_t;

void profiling_stopwatch_init(uint32_t id, const char *name);
void profiling_start(uint32_t id);
void profiling_stop(uint32_t id);
bool profiling_get(uint32_t id, profiling_result_t *result);
void profiling_reset(void);

#endif
