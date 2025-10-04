#include <math.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ui.h"
#include "waveforms.h"

#ifndef WAVE_OUTPUT_GPIO
#define WAVE_OUTPUT_GPIO 4
#endif

#ifndef ROTARY_ENCODER_A_GPIO
#define ROTARY_ENCODER_A_GPIO 8
#endif

#ifndef ROTARY_ENCODER_B_GPIO
#define ROTARY_ENCODER_B_GPIO 9
#endif

#ifndef ROTARY_ENCODER_BUTTON_GPIO
#define ROTARY_ENCODER_BUTTON_GPIO 10
#endif

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_RESOLUTION LEDC_TIMER_13_BIT
#define LEDC_BASE_FREQ 200000

#define BLINK_INTERVAL_US 400000
#define UI_REFRESH_INTERVAL_MS 50

static const char *TAG = "app";

static const waveform_definition_t *s_current_waveform_def = NULL;
static size_t s_waveform_length = 0;
static size_t s_waveform_index = 0;
static esp_timer_handle_t s_wave_timer;
static float s_frequency_hz = 1000.0f;

static const int8_t ENCODER_TRANSITIONS[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0,
};

typedef enum {
    INPUT_MODE_WAVEFORM = 0,
    INPUT_MODE_FREQUENCY,
} input_mode_t;

static input_mode_t s_input_mode = INPUT_MODE_WAVEFORM;

static void configure_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ROTARY_ENCODER_A_GPIO) | (1ULL << ROTARY_ENCODER_B_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << ROTARY_ENCODER_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_conf);
}

static void configure_ledc(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz = LEDC_BASE_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    ledc_channel_config_t channel_config = {
        .gpio_num = WAVE_OUTPUT_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

static void IRAM_ATTR wave_timer_callback(void *arg)
{
    const waveform_definition_t *waveform = s_current_waveform_def;
    if (!waveform || !waveform->samples || s_waveform_length == 0) {
        return;
    }
    size_t index = s_waveform_index;
    if (index >= s_waveform_length) {
        index = 0;
    }
    uint32_t duty = waveform->samples[index];
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    index++;
    if (index >= s_waveform_length) {
        index = 0;
    }
    s_waveform_index = index;
}

static void apply_waveform(const waveform_definition_t *waveform)
{
    s_current_waveform_def = waveform;
    if (!waveform) {
        return;
    }
    s_waveform_length = waveform->length;
    s_waveform_index = 0;

    if (!waveform->samples || waveform->length <= 1 || s_frequency_hz <= 0.0f) {
        esp_err_t err = esp_timer_stop(s_wave_timer);
        if (err != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(err);
        }
        uint32_t duty = waveform->samples ? waveform->samples[0] : 0;
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        return;
    }

    float sample_rate = s_frequency_hz * (float)waveform->length;
    if (sample_rate < 1.0f) {
        sample_rate = 1.0f;
    }
    if (sample_rate > 1000000.0f) {
        sample_rate = 1000000.0f;
    }
    uint64_t period_us = (uint64_t)(1000000.0f / sample_rate);
    if (period_us < 10) {
        period_us = 10;
    }
    esp_err_t err = esp_timer_stop(s_wave_timer);
    if (err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_wave_timer, period_us));
}

static void apply_frequency(float frequency_hz)
{
    s_frequency_hz = frequency_hz;
    apply_waveform(s_current_waveform_def);
}

static void init_wave_timer(void)
{
    const esp_timer_create_args_t args = {
        .callback = &wave_timer_callback,
        .name = "wave_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_wave_timer));
}

static int8_t read_encoder_delta(int *last_state)
{
    int current = (gpio_get_level(ROTARY_ENCODER_A_GPIO) << 1) | gpio_get_level(ROTARY_ENCODER_B_GPIO);
    int index = ((*last_state & 0x03) << 2) | (current & 0x03);
    int8_t delta = ENCODER_TRANSITIONS[index];
    *last_state = current;
    return delta;
}

static bool read_button_pressed(bool *last_level, uint32_t *press_ticks)
{
    bool level = gpio_get_level(ROTARY_ENCODER_BUTTON_GPIO);
    bool pressed = false;
    if (*last_level && !level) {
        *press_ticks = xTaskGetTickCount();
    } else if (!*last_level && level) {
        uint32_t elapsed = xTaskGetTickCount() - *press_ticks;
        if (elapsed > pdMS_TO_TICKS(20)) {
            pressed = true;
        }
    }
    *last_level = level;
    return pressed;
}

static float clamp_frequency(float value)
{
    if (value < 1.0f) {
        return 1.0f;
    }
    if (value > 20000.0f) {
        return 20000.0f;
    }
    return value;
}

static float compute_frequency_step(float current)
{
    if (current < 100.0f) {
        return 1.0f;
    }
    if (current < 1000.0f) {
        return 10.0f;
    }
    if (current < 5000.0f) {
        return 50.0f;
    }
    return 100.0f;
}

void app_main(void)
{
    configure_gpio();
    configure_ledc();
    init_wave_timer();

    ui_context_t *ui = ui_create();
    if (!ui) {
        ESP_LOGE(TAG, "Не удалось инициализировать UI");
        return;
    }

    waveform_id_t waveform_id = WAVEFORM_SINE;
    const waveform_definition_t *waveform = waveforms_get_definition(waveform_id);
    apply_waveform(waveform);

    int encoder_state = (gpio_get_level(ROTARY_ENCODER_A_GPIO) << 1) | gpio_get_level(ROTARY_ENCODER_B_GPIO);
    int8_t accumulated = 0;
    bool last_button_level = gpio_get_level(ROTARY_ENCODER_BUTTON_GPIO);
    uint32_t press_ticks = 0;

    int64_t last_blink_toggle = esp_timer_get_time();
    bool blink_state = true;

    waveform_id_t last_waveform_id = WAVEFORM_COUNT;
    float last_frequency = -1.0f;
    bool last_edit_mode = false;
    bool last_blink_state = false;

    while (true) {
        int8_t delta = read_encoder_delta(&encoder_state);
        if (delta != 0) {
            accumulated += delta;
        }
        if (accumulated >= 4) {
            if (s_input_mode == INPUT_MODE_WAVEFORM) {
                waveform_id = (waveform_id + 1) % WAVEFORM_COUNT;
                waveform = waveforms_get_definition(waveform_id);
                apply_waveform(waveform);
            } else {
                float step = compute_frequency_step(s_frequency_hz);
                s_frequency_hz = clamp_frequency(s_frequency_hz + step);
                apply_frequency(s_frequency_hz);
            }
            accumulated = 0;
        } else if (accumulated <= -4) {
            if (s_input_mode == INPUT_MODE_WAVEFORM) {
                waveform_id = (waveform_id + WAVEFORM_COUNT - 1) % WAVEFORM_COUNT;
                waveform = waveforms_get_definition(waveform_id);
                apply_waveform(waveform);
            } else {
                float step = compute_frequency_step(s_frequency_hz);
                s_frequency_hz = clamp_frequency(s_frequency_hz - step);
                apply_frequency(s_frequency_hz);
            }
            accumulated = 0;
        }

        bool pressed = read_button_pressed(&last_button_level, &press_ticks);
        if (pressed) {
            if (s_input_mode == INPUT_MODE_WAVEFORM) {
                s_input_mode = INPUT_MODE_FREQUENCY;
            } else {
                s_input_mode = INPUT_MODE_WAVEFORM;
            }
        }

        int64_t now = esp_timer_get_time();
        if (now - last_blink_toggle > BLINK_INTERVAL_US) {
            blink_state = !blink_state;
            last_blink_toggle = now;
        }

        bool editing = (s_input_mode == INPUT_MODE_FREQUENCY);

        if (waveform_id != last_waveform_id || s_frequency_hz != last_frequency || editing != last_edit_mode || (editing && blink_state != last_blink_state)) {
            ui_set_waveform(ui, waveform);
            ui_set_frequency(ui, s_frequency_hz);
            ui_set_edit_mode(ui, editing, blink_state);
            ui_flush(ui);

            last_waveform_id = waveform_id;
            last_frequency = s_frequency_hz;
            last_edit_mode = editing;
            last_blink_state = blink_state;
        }

        vTaskDelay(pdMS_TO_TICKS(UI_REFRESH_INTERVAL_MS));
    }
}

