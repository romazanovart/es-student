#include "api.h"
#include "led.h"
#include "pico/stdlib.h"
#include "profiling.h"
#include "stdio-text-protocol.h"

#define BLINK_HALF_PERIOD_US 500000

static uint64_t last_toggle_us;

static void blink(void)
{
    uint64_t now_us = time_us_64();

    if (now_us - last_toggle_us >= BLINK_HALF_PERIOD_US)
    {
        last_toggle_us = now_us;
        led_toggle();
    }
}

int main(void)
{
    stdio_init_all();
    led_init();
    stdio_text_protocol_init();
    profiling_init();

    while (true)
    {
        profiling_iteration();
        blink();

        const command_t *command = stdio_text_protocol_handle();
        if (command != NULL)
        {
            api_handle(command);
        }
    }
}
