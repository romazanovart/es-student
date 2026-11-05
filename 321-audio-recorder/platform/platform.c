#include "platform/platform.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"

#define LED_PIN 25
#define BUTTON_PIN 15
#define ADC_PIN 26
#define CHARGE_PIN 22
#define ADC_CHANNEL 0
#define ADC_TEMP_CHANNEL 4
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096

void platform_init(void)
{
    stdio_init_all();
    gpio_init(CHARGE_PIN);
    gpio_put(CHARGE_PIN, false);
    gpio_set_dir(CHARGE_PIN, GPIO_OUT);
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_set_temp_sensor_enabled(true);
}

float platform_temp_read(void)
{
    adc_select_input(ADC_TEMP_CHANNEL);
    float voltage = adc_read() * ADC_VREF_V / ADC_LEVELS;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return adc_read() * ADC_VREF_V / ADC_LEVELS;
}

void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(void)
{
    return !gpio_get(BUTTON_PIN);
}

void platform_charge_set(bool on)
{
    gpio_put(CHARGE_PIN, on);
}

static repeating_timer_t blink_timer;
static bool blink_running;
static volatile bool blink_on;
static volatile uint32_t blink_toggles;

static bool on_blink(repeating_timer_t *timer)
{
    (void)timer;
    blink_on = !blink_on;
    gpio_put(LED_PIN, blink_on);
    blink_toggles++;
    return true;
}

bool platform_blink_start(uint32_t period_ms)
{
    if (period_ms < 10 || period_ms > 60000)
        return false;

    platform_blink_stop();
    blink_toggles = 0;
    blink_running = add_repeating_timer_ms(-(int32_t)period_ms, on_blink, NULL, &blink_timer);
    return blink_running;
}

uint32_t platform_blink_stop(void)
{
    if (blink_running)
    {
        cancel_repeating_timer(&blink_timer);
        blink_running = false;
    }
    blink_on = false;
    gpio_put(LED_PIN, false);
    return blink_toggles;
}

static repeating_timer_t capture_timer;
static uint16_t capture_samples[PLATFORM_CAPTURE_MAX];
static uint32_t capture_times[PLATFORM_CAPTURE_MAX];
static uint32_t capture_count;
static uint32_t capture_period_us;
static volatile uint32_t capture_index;
static volatile uint32_t capture_handler_us;
static volatile bool capture_running;

static bool on_capture(repeating_timer_t *timer)
{
    (void)timer;
    uint32_t start = time_us_32();
    capture_times[capture_index] = start;
    capture_samples[capture_index++] = adc_read();
    capture_handler_us += time_us_32() - start;
    if (capture_index < capture_count)
        return true;
    capture_running = false;
    return false;
}

bool platform_adc_busy(void)
{
    return capture_running;
}

bool platform_capture_start(uint32_t count, uint32_t period_us)
{
    if (platform_adc_busy() || count == 0 || count > PLATFORM_CAPTURE_MAX || period_us < 10)
        return false;
    adc_select_input(ADC_CHANNEL);
    capture_count = count;
    capture_period_us = period_us;
    capture_index = 0;
    capture_handler_us = 0;
    capture_running = true;
    if (!add_repeating_timer_us(-(int64_t)period_us, on_capture, NULL, &capture_timer))
    {
        capture_running = false;
        return false;
    }
    return true;
}

bool platform_capture_done(void)
{
    return !capture_running && capture_count > 0 && capture_index == capture_count;
}

void platform_capture_get(platform_capture_t *capture)
{
    *capture = (platform_capture_t){
        .samples = capture_samples,
        .count = capture_count,
        .period_us = capture_period_us,
    };
    if (capture_count < 2)
        return;
    capture->min_interval_us = UINT32_MAX;
    for (uint32_t i = 1; i < capture_count; i++)
    {
        uint32_t interval = capture_times[i] - capture_times[i - 1];
        if (interval < capture->min_interval_us)
            capture->min_interval_us = interval;
        if (interval > capture->max_interval_us)
            capture->max_interval_us = interval;
    }
    uint32_t elapsed = capture_times[capture_count - 1] - capture_times[0];
    if (elapsed)
        capture->handler_percent = 100.0f * capture_handler_us / elapsed;
}
