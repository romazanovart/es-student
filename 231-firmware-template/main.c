#include "api.h"
#include "button-task.h"
#include "led-task.h"
#include "pico/stdlib.h"
#include "profiling.h"
#include "stdio-text-protocol.h"

int main(void)
{
    stdio_init_all();
    led_task_init();
    button_task_init();
    stdio_text_protocol_init();
    profiling_init();

    while (true)
    {
        profiling_iteration();
        led_task_handle();
        button_task_handle();

        const command_t *command = stdio_text_protocol_handle();
        if (command != NULL)
        {
            api_handle(command);
        }
    }
}
