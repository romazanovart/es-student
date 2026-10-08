#ifndef PI_TASK_H
#define PI_TASK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PI_TASK_NOT_STARTED,
    PI_TASK_RUNNING,
    PI_TASK_READY,
} pi_task_state_t;

typedef struct {
    pi_task_state_t state;
    double value;
    uint64_t time_ms;
    uint32_t terms;
} pi_task_result_t;

bool pi_task_start(uint32_t terms);
void pi_task_handle(void);
void pi_task_get_result(pi_task_result_t *result);

#endif
