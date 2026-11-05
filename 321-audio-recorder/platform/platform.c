#include "platform/platform.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

#define LED_PIN 25
#define BUTTON_PIN 15
#define ADC_PIN 26
#define TONE_PIN 20
#define TONE_COUNT_HZ 1000000
#define CHARGE_PIN 22
#define ADC_CHANNEL 0
#define ADC_TEMP_CHANNEL 4
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096

static int block_channel;
static uint16_t block_samples[PLATFORM_BLOCK_MAX];
static uint32_t block_start_us;
static volatile uint32_t block_end_us;
static volatile bool block_ready;
static volatile bool block_running;

static void adc_one_shot(void)
{
    adc_run(false);
    adc_fifo_drain();
    adc_fifo_setup(false, false, 0, false, false);
    adc_set_clkdiv(0);
}

static void on_block_done(void)
{
    dma_channel_acknowledge_irq1(block_channel);
    block_end_us = time_us_32();
    adc_one_shot();
    block_ready = true;
    block_running = false;
}

bool platform_block_start(uint32_t count)
{
    if (platform_adc_busy() || count == 0 || count > PLATFORM_BLOCK_MAX)
        return false;

    adc_select_input(ADC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(48000000.0f / PLATFORM_SAMPLE_RATE_HZ - 1.0f);
    dma_channel_config config = dma_channel_get_default_config(block_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, DREQ_ADC);
    block_ready = false;
    block_running = true;
    dma_channel_configure(block_channel, &config, block_samples, &adc_hw->fifo, count, true);
    block_start_us = time_us_32();
    adc_run(true);
    return true;
}

bool platform_block_done(void)
{
    return block_ready;
}

const uint16_t *platform_block_samples(void)
{
    return block_samples;
}

uint32_t platform_block_time_us(void)
{
    return block_end_us - block_start_us;
}

static bool stream_running;

static int stream_channels[2];
static uint8_t stream_buffers[2][PLATFORM_STREAM_BYTES] __attribute__((aligned(4)));
static volatile bool stream_ready[2];
static volatile uint32_t stream_overruns;
static uint32_t stream_samples;
static uint32_t stream_next;

static void on_stream_buffer(void)
{
    for (int i = 0; i < 2; i++)
    {
        if (dma_channel_get_irq0_status(stream_channels[i]))
        {
            dma_channel_acknowledge_irq0(stream_channels[i]);

            dma_channel_set_write_addr(stream_channels[i], stream_buffers[i], false);

            if (stream_ready[(i + 1) % 2])
            {
                stream_overruns++;
            }
            stream_ready[i] = true;
        }
    }
}

bool platform_stream_start(uint8_t bits)
{
    if (platform_adc_busy() || bits != 16)
    {
        return false;
    }

    stream_samples = PLATFORM_STREAM_BYTES / 2;
    adc_select_input(ADC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(48000000.0f / PLATFORM_SAMPLE_RATE_HZ - 1.0f);

    for (int i = 0; i < 2; i++)
    {
        int next_channel = stream_channels[(i + 1) % 2];

        dma_channel_config cfg = dma_channel_get_default_config(stream_channels[i]);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_dreq(&cfg, DREQ_ADC);
        channel_config_set_chain_to(&cfg, next_channel);
        dma_channel_configure(stream_channels[i], &cfg, stream_buffers[i], &adc_hw->fifo,
                              stream_samples, false);
        dma_channel_set_irq0_enabled(stream_channels[i], true);
        stream_ready[i] = false;
    }

    stream_overruns = 0;
    stream_next = 0;
    stream_running = true;
    dma_channel_start(stream_channels[0]);
    adc_run(true);
    return true;
}

void platform_stream_stop(void)
{
    if (!stream_running)
    {
        return;
    }

    adc_run(false);

    for (int i = 0; i < 2; i++)
    {
        dma_channel_config cfg = dma_get_channel_config(stream_channels[i]);
        channel_config_set_chain_to(&cfg, stream_channels[i]);
        dma_channel_set_config(stream_channels[i], &cfg, false);
    }

    for (int i = 0; i < 2; i++)
    {
        dma_channel_set_irq0_enabled(stream_channels[i], false);
        dma_channel_abort(stream_channels[i]);
        dma_channel_acknowledge_irq0(stream_channels[i]);
    }
    adc_one_shot();
    stream_running = false;
}

void platform_write(const void *data, uint32_t bytes)
{
    stdio_put_string((const char *)data, (int)bytes, false, false);
}

const uint8_t *platform_stream_next(uint32_t *samples)
{
    if (!stream_running || !stream_ready[stream_next])
        return NULL;
    *samples = stream_samples;
    return stream_buffers[stream_next];
}

void platform_stream_release(void)
{
    stream_ready[stream_next] = false;
    stream_next ^= 1;
}

uint32_t platform_stream_overruns(void)
{
    return stream_overruns;
}

bool platform_tone_set(uint32_t freq_hz)
{
    if (freq_hz == 0)
    {
        pwm_set_gpio_level(TONE_PIN, 0);
        return true;
    }
    if (freq_hz < 20 || freq_hz > 20000)
        return false;
    uint32_t period = TONE_COUNT_HZ / freq_hz;
    pwm_set_wrap(pwm_gpio_to_slice_num(TONE_PIN), period - 1);
    pwm_set_gpio_level(TONE_PIN, period / 2);
    return true;
}

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
    block_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq1_enabled(block_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, on_block_done);
    irq_set_enabled(DMA_IRQ_1, true);
    stream_channels[0] = dma_claim_unused_channel(true);
    stream_channels[1] = dma_claim_unused_channel(true);
    irq_set_exclusive_handler(DMA_IRQ_0, on_stream_buffer);
    irq_set_enabled(DMA_IRQ_0, true);
    gpio_set_function(TONE_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(TONE_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, (float)clock_get_hz(clk_sys) / TONE_COUNT_HZ);
    pwm_init(slice, &config, false);
    pwm_set_gpio_level(TONE_PIN, 0);
    pwm_set_enabled(slice, true);
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
    return capture_running || block_running || stream_running;
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
