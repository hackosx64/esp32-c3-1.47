#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "driver/i2c.h"

#ifndef SSD1306_WIDTH
#define SSD1306_WIDTH 128
#endif

#ifndef SSD1306_HEIGHT
#define SSD1306_HEIGHT 64
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t port;
    uint8_t address;
    uint8_t *buffer;
    size_t buffer_size;
} ssd1306_t;

esp_err_t ssd1306_init(ssd1306_t *dev, i2c_port_t port, uint8_t address, gpio_num_t sda, gpio_num_t scl, uint32_t clk_speed_hz);

void ssd1306_deinit(ssd1306_t *dev);

void ssd1306_clear(ssd1306_t *dev);

void ssd1306_set_pixel(ssd1306_t *dev, uint16_t x, uint16_t y, bool on);

void ssd1306_draw_line(ssd1306_t *dev, int x0, int y0, int x1, int y1, bool on);

void ssd1306_draw_rect(ssd1306_t *dev, int x, int y, int w, int h, bool on);

void ssd1306_fill_rect(ssd1306_t *dev, int x, int y, int w, int h, bool on);

void ssd1306_draw_text(ssd1306_t *dev, int x, int y, const char *text, bool on);

esp_err_t ssd1306_flush(ssd1306_t *dev);

#ifdef __cplusplus
}
#endif

