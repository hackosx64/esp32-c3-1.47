#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "waveforms.h"

typedef struct ui_context ui_context_t;

ui_context_t *ui_create(void);

void ui_destroy(ui_context_t *ctx);

void ui_set_waveform(ui_context_t *ctx, const waveform_definition_t *waveform);

void ui_set_frequency(ui_context_t *ctx, float frequency_hz);

void ui_set_edit_mode(ui_context_t *ctx, bool editing, bool blink_state);

void ui_flush(ui_context_t *ctx);

