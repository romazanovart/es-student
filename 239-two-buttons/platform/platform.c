#include "platform/platform.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define LED_PIN 25

static const uint button_pins[PLATFORM_BUTTON_COUNT] = {15, 14};

void platform_init(void)
{
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    for (uint button = 0; button < PLATFORM_BUTTON_COUNT; button++)
    {
        gpio_init(button_pins[button]);
        gpio_set_dir(button_pins[button], GPIO_IN);
        gpio_pull_up(button_pins[button]);
    }
}

void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(platform_button_t button)
{
    return !gpio_get(button_pins[button]);
}
