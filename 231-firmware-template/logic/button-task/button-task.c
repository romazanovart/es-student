#include "button-task/button-task.h"

#include <stddef.h>
#include "platform/platform.h"
#include "systime/systime.h"

#define DEBOUNCE_US 20000

typedef enum {
    BUTTON_STATE_RELEASED,
    BUTTON_STATE_PRESSING,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_RELEASING,
} button_state_t;

static button_state_t button_state;
static uint64_t transition_started_us;
static uint32_t press_count;
static button_task_callback_t press_callback;

void button_task_init(button_task_callback_t on_press)
{
    button_state = BUTTON_STATE_RELEASED;
    press_count = 0;
    press_callback = on_press;
}

void button_task_handle(void)
{
    bool pressed = platform_button_read();
    uint64_t now_us = systime_us();

    switch (button_state)
    {
        case BUTTON_STATE_RELEASED:
            if (pressed)
            {
                transition_started_us = now_us;
                button_state = BUTTON_STATE_PRESSING;
            }
            break;
        case BUTTON_STATE_PRESSING:
            if (!pressed)
            {
                button_state = BUTTON_STATE_RELEASED;
            }
            else if (now_us - transition_started_us >= DEBOUNCE_US)
            {
                button_state = BUTTON_STATE_PRESSED;
                press_count++;
                if (press_callback != NULL)
                {
                    press_callback();
                }
            }
            break;
        case BUTTON_STATE_PRESSED:
            if (!pressed)
            {
                transition_started_us = now_us;
                button_state = BUTTON_STATE_RELEASING;
            }
            break;
        case BUTTON_STATE_RELEASING:
            if (pressed)
            {
                button_state = BUTTON_STATE_PRESSED;
            }
            else if (now_us - transition_started_us >= DEBOUNCE_US)
            {
                button_state = BUTTON_STATE_RELEASED;
            }
            break;
    }
}

bool button_task_is_pressed(void)
{
    return button_state == BUTTON_STATE_PRESSED ||
           button_state == BUTTON_STATE_RELEASING;
}

uint32_t button_task_get_press_count(void)
{
    return press_count;
}
