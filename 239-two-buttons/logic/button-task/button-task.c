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

typedef struct {
    button_state_t state;
    uint64_t transition_started_us;
    uint32_t press_count;
    button_task_callback_t callback;
} button_t;

static button_t buttons[PLATFORM_BUTTON_COUNT];

void button_task_init(platform_button_t button, button_task_callback_t on_press)
{
    buttons[button].state = BUTTON_STATE_RELEASED;
    buttons[button].press_count = 0;
    buttons[button].callback = on_press;
}

static void button_handle(platform_button_t number)
{
    button_t *button = &buttons[number];
    bool pressed = platform_button_read(number);
    uint64_t now_us = systime_us();

    switch (button->state)
    {
        case BUTTON_STATE_RELEASED:
            if (pressed)
            {
                button->transition_started_us = now_us;
                button->state = BUTTON_STATE_PRESSING;
            }
            break;
        case BUTTON_STATE_PRESSING:
            if (!pressed)
            {
                button->state = BUTTON_STATE_RELEASED;
            }
            else if (now_us - button->transition_started_us >= DEBOUNCE_US)
            {
                button->state = BUTTON_STATE_PRESSED;
                button->press_count++;
                if (button->callback != NULL)
                {
                    button->callback();
                }
            }
            break;
        case BUTTON_STATE_PRESSED:
            if (!pressed)
            {
                button->transition_started_us = now_us;
                button->state = BUTTON_STATE_RELEASING;
            }
            break;
        case BUTTON_STATE_RELEASING:
            if (pressed)
            {
                button->state = BUTTON_STATE_PRESSED;
            }
            else if (now_us - button->transition_started_us >= DEBOUNCE_US)
            {
                button->state = BUTTON_STATE_RELEASED;
            }
            break;
    }
}

void button_task_handle(void)
{
    for (platform_button_t button = PLATFORM_BUTTON_1;
         button < PLATFORM_BUTTON_COUNT; button++)
    {
        button_handle(button);
    }
}

bool button_task_is_pressed(platform_button_t button)
{
    return buttons[button].state == BUTTON_STATE_PRESSED ||
           buttons[button].state == BUTTON_STATE_RELEASING;
}

uint32_t button_task_get_press_count(platform_button_t button)
{
    return buttons[button].press_count;
}
