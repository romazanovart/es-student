#include "api.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"
#include "firmware.h"
#include "led-task.h"
#include "pico/stdlib.h"
#include "profiling.h"

#define CALC_PI_DEFAULT_TERMS 1000000u

typedef void (*api_callback_t)(const command_t *command);

typedef struct
{
    const char *name;
    const char *help;
    api_callback_t callback;
} api_command_t;

static volatile double pi_result;

static const char *led_state_name(led_state_t state)
{
    switch (state)
    {
        case LED_STATE_OFF:
            return "off";
        case LED_STATE_ON:
            return "on";
        case LED_STATE_BLINK:
            return "blink";
        default:
            return "unknown";
    }
}

static void print_led(void)
{
    printf("led: %s, period %u ms\n",
           led_state_name(led_task_get_state()),
           (unsigned)led_task_get_period_ms());
}

static bool parse_u32(const char *text, uint32_t *result)
{
    if (text == NULL || *text == '\0' || *text == '-')
    {
        return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || value > UINT32_MAX)
    {
        return false;
    }

    *result = (uint32_t)value;
    return true;
}

static double calc_pi(uint32_t terms)
{
    double sum = 0.0;
    double sign = 1.0;

    for (uint32_t k = 0; k < terms; k++)
    {
        sum += sign / (2.0 * k + 1.0);
        sign = -sign;
    }

    return sum * 4.0;
}

static void command_info(const command_t *command)
{
    (void)command;
    printf("name: %s\n", FIRMWARE_NAME);
    printf("version: %s\n", FIRMWARE_VERSION);
    device_info();
}

static void command_uptime(const command_t *command)
{
    (void)command;
    printf("uptime: %llu ms\n", (unsigned long long)(time_us_64() / 1000));
}

static void command_calc_pi(const command_t *command)
{
    uint32_t terms = CALC_PI_DEFAULT_TERMS;
    if (command->argc > 1 ||
        (command->argc == 1 && !parse_u32(command->argv[0], &terms)))
    {
        printf("error: usage calc_pi [terms]\n");
        return;
    }
    if (terms == 0)
    {
        printf("error: terms must be greater than 0\n");
        return;
    }

    uint64_t start_us = time_us_64();
    pi_result = calc_pi(terms);
    uint64_t spent_us = time_us_64() - start_us;
    printf("pi: %.9f\n", pi_result);
    printf("time: %llu ms\n", (unsigned long long)(spent_us / 1000));
}

static void command_main_time_exec(const command_t *command)
{
    (void)command;
    printf("iteration avg %.2f us, max %u us\n",
           profiling_avg_us(), (unsigned)profiling_max_us());
}

static void command_main_time_reset(const command_t *command)
{
    (void)command;
    profiling_reset_max();
    printf("max reset\n");
}

static void command_led_enable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_ON);
    print_led();
}

static void command_led_disable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_OFF);
    print_led();
}

static void command_led_blink(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_BLINK);
    print_led();
}

static void command_led_period(const command_t *command)
{
    uint32_t period_ms;
    if (command->argc != 1 || !parse_u32(command->argv[0], &period_ms))
    {
        printf("error: usage led_period <period_ms>\n");
        return;
    }
    if (!led_task_set_period_ms(period_ms))
    {
        printf("error: period_ms must be greater than 0\n");
        return;
    }
    print_led();
}

static const api_command_t commands[] = {
    { "info", "device passport", command_info },
    { "uptime", "time since reset", command_uptime },
    { "calc_pi", "calculate pi: calc_pi [terms]", command_calc_pi },
    { "main_time_exec", "superloop iteration: average and maximum", command_main_time_exec },
    { "main_time_reset", "reset maximum iteration", command_main_time_reset },
    { "led_enable", "turn LED on", command_led_enable },
    { "led_disable", "turn LED off", command_led_disable },
    { "led_blink", "blink LED", command_led_blink },
    { "led_period", "set blink period: led_period <period_ms>", command_led_period },
};

static void command_help(void)
{
    printf("%-16s %s\n", "help", "list of commands");
    for (uint32_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        printf("%-16s %s\n", commands[i].name, commands[i].help);
    }
}

void api_handle(const command_t *command)
{
    if (command->truncated)
    {
        printf("error: command longer than 63 characters or 4 arguments\n");
        return;
    }

    if (strcmp(command->name, "help") == 0)
    {
        command_help();
        return;
    }

    for (uint32_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        if (strcmp(command->name, commands[i].name) == 0)
        {
            commands[i].callback(command);
            return;
        }
    }

    printf("error: unknown command '%s', try help\n", command->name);
}
