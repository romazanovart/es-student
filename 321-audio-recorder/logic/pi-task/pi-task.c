#include "pi-task/pi-task.h"

#include "systime/systime.h"

/* One term takes about 4.7 us, so 100 terms fit into 0.5 ms. */
#define PI_TERMS_PER_CALL 100

static pi_task_state_t pi_state;
static uint32_t pi_terms;
static uint32_t pi_k;
static double pi_sum;
static double pi_value;
static uint64_t pi_started_us;
static uint64_t pi_elapsed_ms;

bool pi_task_start(uint32_t terms)
{
    if (pi_state == PI_TASK_RUNNING)
    {
        return false;
    }

    pi_terms = terms;
    pi_k = 0;
    pi_sum = 0.0;
    pi_started_us = systime_us();
    pi_state = PI_TASK_RUNNING;
    return true;
}

void pi_task_handle(void)
{
    if (pi_state != PI_TASK_RUNNING)
    {
        return;
    }

    uint32_t end = pi_k + PI_TERMS_PER_CALL;
    if (end < pi_k || end > pi_terms)
    {
        end = pi_terms;
    }

    for (; pi_k < end; pi_k++)
    {
        double term = 1.0 / (2.0 * pi_k + 1.0);
        pi_sum += (pi_k & 1u) ? -term : term;
    }

    if (pi_k == pi_terms)
    {
        pi_value = pi_sum * 4.0;
        pi_elapsed_ms = (systime_us() - pi_started_us) / 1000;
        pi_state = PI_TASK_READY;
    }
}

void pi_task_get_result(pi_task_result_t *result)
{
    result->state = pi_state;
    result->value = pi_value;
    result->time_ms = pi_elapsed_ms;
    result->terms = pi_terms;
}
