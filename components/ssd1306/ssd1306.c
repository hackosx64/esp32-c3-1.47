#include "ssd1306.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"

#include "font5x7.h"

#define TAG "ssd1306"

#define SSD1306_I2C_TIMEOUT_MS 100

#define SSD1306_CONTROL_COMMAND 0x00
#define SSD1306_CONTROL_DATA 0x40

static esp_err_t ssd1306_send_command(ssd1306_t *dev, uint8_t command)
{
    uint8_t buffer[2] = {SSD1306_CONTROL_COMMAND, command};
    return i2c_master_write_to_device(dev->port, dev->address, buffer, sizeof(buffer), pdMS_TO_TICKS(SSD1306_I2C_TIMEOUT_MS));
}

static esp_err_t ssd1306_send_commands(ssd1306_t *dev, const uint8_t *commands, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        ESP_RETURN_ON_ERROR(ssd1306_send_command(dev, commands[i]), TAG, "command 0x%02x failed", commands[i]);
    }
    return ESP_OK;
}

static esp_err_t ssd1306_send_data(ssd1306_t *dev, const uint8_t *data, size_t length)
{
    if (length == 0) {
        return ESP_OK;
    }
    uint8_t buffer[17];
    buffer[0] = SSD1306_CONTROL_DATA;
    if (length > sizeof(buffer) - 1) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(&buffer[1], data, length);
    return i2c_master_write_to_device(dev->port, dev->address, buffer, length + 1, pdMS_TO_TICKS(SSD1306_I2C_TIMEOUT_MS));
}

esp_err_t ssd1306_init(ssd1306_t *dev, i2c_port_t port, uint8_t address, gpio_num_t sda, gpio_num_t scl, uint32_t clk_speed_hz)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(dev, 0, sizeof(*dev));
    dev->port = port;
    dev->address = address;
    dev->buffer_size = (SSD1306_WIDTH * SSD1306_HEIGHT) / 8;
    dev->buffer = (uint8_t *)calloc(dev->buffer_size, sizeof(uint8_t));
    ESP_RETURN_ON_FALSE(dev->buffer, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");

    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = clk_speed_hz,
        .clk_flags = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(port, &config), TAG, "param config");
    ESP_RETURN_ON_ERROR(i2c_driver_install(port, config.mode, 0, 0, 0), TAG, "install driver");

    const uint8_t init_commands[] = {
        0xAE,
        0xD5, 0x80,
        0xA8, SSD1306_HEIGHT - 1,
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,
        0x20, 0x00,
        0xA1,
        0xC8,
        0xDA, (SSD1306_HEIGHT == 64) ? 0x12 : 0x02,
        0x81, 0x7F,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,
        0x2E,
        0xAF,
    };

    ESP_RETURN_ON_ERROR(ssd1306_send_commands(dev, init_commands, sizeof(init_commands)), TAG, "init sequence failed");
    ssd1306_clear(dev);
    return ssd1306_flush(dev);
}

void ssd1306_deinit(ssd1306_t *dev)
{
    if (!dev) {
        return;
    }
    if (dev->buffer) {
        free(dev->buffer);
        dev->buffer = NULL;
    }
    if (dev->port < I2C_NUM_MAX) {
        i2c_driver_delete(dev->port);
    }
}

void ssd1306_clear(ssd1306_t *dev)
{
    if (!dev || !dev->buffer) {
        return;
    }
    memset(dev->buffer, 0x00, dev->buffer_size);
}

static void ssd1306_update_buffer(ssd1306_t *dev, uint16_t x, uint16_t y, bool on)
{
    if (!dev || !dev->buffer) {
        return;
    }
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return;
    }
    size_t index = (y / 8) * SSD1306_WIDTH + x;
    uint8_t mask = 1 << (y % 8);
    if (on) {
        dev->buffer[index] |= mask;
    } else {
        dev->buffer[index] &= ~mask;
    }
}

void ssd1306_set_pixel(ssd1306_t *dev, uint16_t x, uint16_t y, bool on)
{
    ssd1306_update_buffer(dev, x, y, on);
}

void ssd1306_draw_line(ssd1306_t *dev, int x0, int y0, int x1, int y1, bool on)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        ssd1306_set_pixel(dev, x0, y0, on);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void ssd1306_draw_rect(ssd1306_t *dev, int x, int y, int w, int h, bool on)
{
    ssd1306_draw_line(dev, x, y, x + w - 1, y, on);
    ssd1306_draw_line(dev, x, y + h - 1, x + w - 1, y + h - 1, on);
    ssd1306_draw_line(dev, x, y, x, y + h - 1, on);
    ssd1306_draw_line(dev, x + w - 1, y, x + w - 1, y + h - 1, on);
}

void ssd1306_fill_rect(ssd1306_t *dev, int x, int y, int w, int h, bool on)
{
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) {
            ssd1306_set_pixel(dev, xx, yy, on);
        }
    }
}

void ssd1306_draw_text(ssd1306_t *dev, int x, int y, const char *text, bool on)
{
    if (!dev || !dev->buffer || !text) {
        return;
    }
    while (*text) {
        char c = *text;
        if (c < 0x20 || c > 0x7E) {
            c = '?';
        }
        const uint8_t *glyph = &font5x7[(c - 0x20) * 5];
        for (int col = 0; col < 5; ++col) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < 7; ++row) {
                ssd1306_set_pixel(dev, x + col, y + row, false);
            }
            for (int row = 0; row < 7; ++row) {
                bool pixel_on = (bits >> row) & 0x01;
                if (pixel_on && on) {
                    ssd1306_set_pixel(dev, x + col, y + row, true);
                }
            }
        }
        for (int row = 0; row < 7; ++row) {
            ssd1306_set_pixel(dev, x + 5, y + row, false);
        }
        x += 6;
        ++text;
    }
}

esp_err_t ssd1306_flush(ssd1306_t *dev)
{
    if (!dev || !dev->buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t range_commands[] = {
        0x21, 0, SSD1306_WIDTH - 1,
        0x22, 0, (SSD1306_HEIGHT / 8) - 1,
    };
    ESP_RETURN_ON_ERROR(ssd1306_send_commands(dev, range_commands, sizeof(range_commands)), TAG, "set range");

    size_t offset = 0;
    size_t remaining = dev->buffer_size;
    while (remaining) {
        size_t chunk = remaining > 16 ? 16 : remaining;
        ESP_RETURN_ON_ERROR(ssd1306_send_data(dev, dev->buffer + offset, chunk), TAG, "write data");
        offset += chunk;
        remaining -= chunk;
    }
    return ESP_OK;
}

