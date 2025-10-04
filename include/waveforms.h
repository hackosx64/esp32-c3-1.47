#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WAVEFORM_SINE = 0,
    WAVEFORM_TRIANGLE,
    WAVEFORM_SAWTOOTH,
    WAVEFORM_SQUARE,
    WAVEFORM_PWM,
    WAVEFORM_DC,
    WAVEFORM_BURST,
    WAVEFORM_STAIRCASE,
    WAVEFORM_COUNT
} waveform_id_t;

typedef struct {
    const char *name;
    const uint16_t *samples;
    size_t length;
    uint16_t min_value;
    uint16_t max_value;
} waveform_definition_t;

#define WAVEFORM_MAX_SAMPLES 128

const waveform_definition_t *waveforms_get_definition(waveform_id_t id);

const waveform_definition_t *waveforms_get_all(size_t *count);

uint16_t waveforms_sample_scaled(const waveform_definition_t *waveform, size_t index, uint16_t target_max);

#ifdef __cplusplus
}
#endif

