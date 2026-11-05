#pragma once

#include <stdbool.h>
#include <stdint.h>

void platform_init(void);
float platform_voltage_read(void);
float platform_temp_read(void);
void platform_charge_set(bool on);
void platform_led_set(bool on);
bool platform_button_read(void);

bool platform_blink_start(uint32_t period_ms);
uint32_t platform_blink_stop(void);
