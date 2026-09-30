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
#include "memory.h"
#include "command.h"
#include "clock.h"

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

void cmd_enable(void)
{
    led_set(true);
    LOG_INF("led %s\n", led_is_on() ? "on" : "off");
}

void cmd_disable(void)
{
    led_set(false);
    LOG_INF("led %s\n", led_is_on() ? "on" : "off");
}

void cmd_info(void)
{
    device_info();
}

void cmd_version(void)
{
    log_version();
}

void cmd_ping(void)
{
    printf("pong\n");
}

void cmd_mem_info(void)
{
    mem_info();
}

void cmd_fw_info(void)
{
    fw_info();
}

void cmd_dev_info(void)
{
    dev_info();
}

void cmd_boot_info(void)
{
    boot_info();
}

void cmd_clk_info(void)
{
    clk_info();
}

const struct command_t commands[] = {
    { "enable", cmd_enable },
    { "disable", cmd_disable },
    { "info", cmd_info },
    { "version", cmd_version },
    { "ping", cmd_ping },
    { "mem_info", cmd_mem_info },
    { "fw_info", cmd_fw_info },
    { "dev_info", cmd_dev_info },
    { "boot_info", cmd_boot_info },
    { "clk_info", cmd_clk_info },
};

const uint command_count = sizeof(commands) / sizeof(commands[0]);

void handle_command(const char *command)
{
    for (uint i = 0; i < command_count; i++)
    {
        if (strcmp(command, commands[i].name) == 0)
        {
            if (commands[i].handler != NULL)
            {
                commands[i].handler();
            }

            return;
        }
    }

    LOG_ERR("unknown command: %s\n", command);
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

int main(void)
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
