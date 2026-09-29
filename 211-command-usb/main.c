#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "led.h"
#include "log.h"
#include "device.h"

const uint BUTTON_PIN = 15;
const uint DEBOUNCE_MS = 20;

#define LINE_SIZE 32

char line[LINE_SIZE];
uint line_length = 0;

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

void handle_command(const char *command)
{
    if (strcmp(command, "enable") == 0)
    {
        led_set(true);
        LOG_INF("led %s\n", led_is_on() ? "on" : "off");
    }
    else if (strcmp(command, "disable") == 0)
    {
        led_set(false);
        LOG_INF("led %s\n", led_is_on() ? "on" : "off");
    }
    else if (strcmp(command, "version") == 0)
    {
        log_version();
    }
    else if (strcmp(command, "info") == 0)
    {
        device_info();
    }
    else
    {
        LOG_ERR("unknown command: %s\n", command);
    }
}

void read_line(void)
{
    int symbol = getchar_timeout_us(0);

    if (symbol == PICO_ERROR_TIMEOUT)
    {
        return;
    }

    if (symbol == '\r' || symbol == '\n')
    {
        putchar('\n');
        line[line_length] = '\0';

        if (line_length > 0)
        {
            LOG_DBG("got %s\n", line);
            handle_command(line);
        }

        line_length = 0;
        return;
    }

    if (line_length + 1 < LINE_SIZE)
    {
        line[line_length] = (char)symbol;
        line_length = line_length + 1;
        putchar(symbol);
    }
}

int main()
{
    stdio_init_all();

    led_init();

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    bool previous = false;

    while (1)
    {
        bool current = get_button_debounce(BUTTON_PIN);

        if (previous == true && current == false)
        {
            led_toggle();
            LOG_INF("led %s\n", led_is_on() ? "on" : "off");
        }

        previous = current;

        read_line();
    }
}
