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

#define PLATFORM_CAPTURE_MAX 2000

typedef struct {
    const uint16_t *samples;
    uint32_t count;
    uint32_t period_us;
    uint32_t min_interval_us;
    uint32_t max_interval_us;
    float handler_percent;
} platform_capture_t;

bool platform_adc_busy(void);
bool platform_capture_start(uint32_t count, uint32_t period_us);
bool platform_capture_done(void);
void platform_capture_get(platform_capture_t *capture);

#define PLATFORM_SAMPLE_RATE_HZ 44100
#define PLATFORM_BLOCK_MAX 8192

bool platform_block_start(uint32_t count);
bool platform_block_done(void);
const uint16_t *platform_block_samples(void);
uint32_t platform_block_time_us(void);
