#include "platform/platform.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"

#define LED_PIN 25
#define BUTTON_PIN 15
#define ADC_PIN 26
#define ADC_CHANNEL 0
#define ADC_TEMP_CHANNEL 4
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096

void platform_init(void)
{
    stdio_init_all();
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
