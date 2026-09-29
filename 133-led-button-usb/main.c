#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

const uint LED_PIN = 25;
const uint BUTTON_PIN = 15;
const uint DEBOUNCE_MS = 20;

bool __no_inline_not_in_flash_func(get_bootsel_button)(void)
{
    const uint pin = 1;
    uint32_t interrupts = save_and_disable_interrupts();

    hw_write_masked(
        &ioqspi_hw->io[pin].ctrl,
        GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS
    );

    for (volatile uint delay = 0; delay < 1000; ++delay)
    {
    }

    bool pressed = !(sio_hw->gpio_hi_in & (1u << pin));

    hw_write_masked(
        &ioqspi_hw->io[pin].ctrl,
        GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS
    );

    restore_interrupts(interrupts);
    return pressed;
}

bool get_button_debounce(uint pin)
{
    bool state = gpio_get(pin) && !get_bootsel_button();
    sleep_ms(DEBOUNCE_MS);
    return state && gpio_get(pin) && !get_bootsel_button();
}

void set_led(bool on)
{
    gpio_put(LED_PIN, on);
    printf("led %s\n", on ? "on" : "off");
}

int main()
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    bool led = false;
    bool previous = false;

    while (1)
    {
        bool current = get_button_debounce(BUTTON_PIN);

        if (previous == true && current == false)
        {
            led = !led;
            set_led(led);
        }

        previous = current;
    }
}
