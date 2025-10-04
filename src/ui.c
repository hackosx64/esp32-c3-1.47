#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "driver/gpio.h"

#include "ssd1306.h"

#ifndef SSD1306_SDA_GPIO
#define SSD1306_SDA_GPIO GPIO_NUM_6
#endif

#ifndef SSD1306_SCL_GPIO
#define SSD1306_SCL_GPIO GPIO_NUM_7
#endif

#define I2C_PORT I2C_NUM_0
#define I2C_FREQ_HZ 400000

struct ui_context {
    ssd1306_t display;
    const waveform_definition_t *waveform;
    float frequency_hz;
    bool editing_frequency;
    bool blink_visible;
    bool valid;
};

static void ui_draw_waveform_preview(ui_context_t *ctx)
{
    const int margin = 2;
    const int title_height = 10;
    const int preview_top = title_height + margin;
    const int preview_height = SSD1306_HEIGHT - preview_top - 16 - margin;
    const int preview_bottom = preview_top + preview_height - 1;
    const int preview_left = margin;
    const int preview_width = SSD1306_WIDTH - (margin * 2);

    ssd1306_fill_rect(&ctx->display, preview_left, preview_top, preview_width, preview_height, false);
    ssd1306_draw_rect(&ctx->display, preview_left, preview_top, preview_width, preview_height, true);
    if (!ctx->waveform || preview_width <= 2) {
        return;
    }

    int last_x = preview_left + 1;
    int last_y = preview_bottom;
    size_t waveform_length = ctx->waveform->length ? ctx->waveform->length : 1;
    for (int x = 0; x < preview_width - 2; ++x) {
        size_t idx = (size_t)((float)x / (float)(preview_width - 3) * (float)(waveform_length - 1));
        uint16_t sample = waveforms_sample_scaled(ctx->waveform, idx, preview_height - 2);
        int y = preview_bottom - 1 - sample;
        int actual_x = preview_left + 1 + x;
        ssd1306_draw_line(&ctx->display, last_x, last_y, actual_x, y, true);
        last_x = actual_x;
        last_y = y;
    }
}

static void ui_draw_text_line(ui_context_t *ctx, int x, int y, const char *text)
{
    ssd1306_draw_text(&ctx->display, x, y, text, true);
}

ui_context_t *ui_create(void)
{
    ui_context_t *ctx = calloc(1, sizeof(ui_context_t));
    if (!ctx) {
        return NULL;
    }
    if (ssd1306_init(&ctx->display, I2C_PORT, 0x3C, SSD1306_SDA_GPIO, SSD1306_SCL_GPIO, I2C_FREQ_HZ) != ESP_OK) {
        free(ctx);
        return NULL;
    }
    ctx->valid = true;
    return ctx;
}

void ui_destroy(ui_context_t *ctx)
{
    if (!ctx) {
        return;
    }
    ssd1306_deinit(&ctx->display);
    free(ctx);
}

void ui_set_waveform(ui_context_t *ctx, const waveform_definition_t *waveform)
{
    if (!ctx) {
        return;
    }
    ctx->waveform = waveform;
}

void ui_set_frequency(ui_context_t *ctx, float frequency_hz)
{
    if (!ctx) {
        return;
    }
    ctx->frequency_hz = frequency_hz;
}

void ui_set_edit_mode(ui_context_t *ctx, bool editing, bool blink_state)
{
    if (!ctx) {
        return;
    }
    ctx->editing_frequency = editing;
    ctx->blink_visible = blink_state;
}

static void ui_draw_header(ui_context_t *ctx)
{
    if (!ctx->waveform) {
        return;
    }
    ssd1306_fill_rect(&ctx->display, 0, 0, SSD1306_WIDTH, 10, false);
    ui_draw_text_line(ctx, 2, 1, ctx->waveform->name);
}

static void ui_draw_frequency(ui_context_t *ctx)
{
    const int footer_height = 16;
    const int footer_top = SSD1306_HEIGHT - footer_height;
    ssd1306_fill_rect(&ctx->display, 0, footer_top, SSD1306_WIDTH, footer_height, false);

    if (ctx->editing_frequency && !ctx->blink_visible) {
        return;
    }

    char label[32];
    float freq = ctx->frequency_hz;
    const char *unit = "Гц";
    if (freq >= 1000.0f) {
        freq /= 1000.0f;
        unit = "кГц";
    }
    snprintf(label, sizeof(label), "Частота: %.2f %s", freq, unit);
    ui_draw_text_line(ctx, 2, footer_top + 4, label);
}

void ui_flush(ui_context_t *ctx)
{
    if (!ctx || !ctx->valid) {
        return;
    }
    ssd1306_clear(&ctx->display);
    ui_draw_header(ctx);
    ui_draw_waveform_preview(ctx);
    ui_draw_frequency(ctx);
    ssd1306_flush(&ctx->display);
}

