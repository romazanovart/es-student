#include "profiling/profiling.h"

#include <stddef.h>
#include "pico/stdlib.h"

typedef struct {
    const char *name;
    uint32_t started_us;
    uint32_t total_us;
    uint32_t count;
    uint32_t max_us;
} stopwatch_t;

static stopwatch_t stopwatches[PROFILING_STOPWATCH_COUNT];

void profiling_stopwatch_init(uint32_t id, const char *name)
{
    if (id >= PROFILING_STOPWATCH_COUNT)
    {
        return;
    }
    stopwatches[id].name = name;
    stopwatches[id].started_us = 0;
    stopwatches[id].total_us = 0;
    stopwatches[id].count = 0;
    stopwatches[id].max_us = 0;
}

#if PROFILING_ENABLED
void profiling_start(uint32_t id)
{
    if (id < PROFILING_STOPWATCH_COUNT && stopwatches[id].name != NULL)
    {
        stopwatches[id].started_us = time_us_32();
    }
}

void profiling_stop(uint32_t id)
{
    if (id >= PROFILING_STOPWATCH_COUNT || stopwatches[id].name == NULL)
    {
        return;
    }

    uint32_t elapsed_us = time_us_32() - stopwatches[id].started_us;
    stopwatches[id].total_us += elapsed_us;
    stopwatches[id].count++;
    if (elapsed_us > stopwatches[id].max_us)
    {
        stopwatches[id].max_us = elapsed_us;
    }
}
#endif

bool profiling_get(uint32_t id, profiling_result_t *result)
{
    if (id >= PROFILING_STOPWATCH_COUNT || stopwatches[id].name == NULL)
    {
        return false;
    }

    result->name = stopwatches[id].name;
    result->count = stopwatches[id].count;
    result->mean_us = stopwatches[id].count == 0 ? 0.0f :
                      (float)stopwatches[id].total_us / stopwatches[id].count;
    result->max_us = stopwatches[id].max_us;
    return true;
}

void profiling_reset(void)
{
    for (uint32_t id = 0; id < PROFILING_STOPWATCH_COUNT; id++)
    {
        stopwatches[id].total_us = 0;
        stopwatches[id].count = 0;
        stopwatches[id].max_us = 0;
    }
}
