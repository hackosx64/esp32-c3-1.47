#include "waveforms.h"

#include <assert.h>

#define TABLE_LENGTH WAVEFORM_MAX_SAMPLES

#include "waveform_tables.inc"

static const uint16_t _dc_table[1] = {4095};

static const waveform_definition_t _waveforms[WAVEFORM_COUNT] = {
    [WAVEFORM_SINE] = {
        .name = "Sine",
        .samples = _sine_table,
        .length = TABLE_LENGTH,
        .min_value = 1,
        .max_value = 8190,
    },
    [WAVEFORM_TRIANGLE] = {
        .name = "Triangle",
        .samples = _triangle_table,
        .length = TABLE_LENGTH,
        .min_value = 0,
        .max_value = 8191,
    },
    [WAVEFORM_SAWTOOTH] = {
        .name = "Saw",
        .samples = _saw_table,
        .length = TABLE_LENGTH,
        .min_value = 0,
        .max_value = 8191,
    },
    [WAVEFORM_SQUARE] = {
        .name = "Square",
        .samples = _square_table,
        .length = TABLE_LENGTH,
        .min_value = 0,
        .max_value = 8191,
    },
    [WAVEFORM_PWM] = {
        .name = "PWM 25%",
        .samples = _pwm_table,
        .length = TABLE_LENGTH,
        .min_value = 0,
        .max_value = 8191,
    },
    [WAVEFORM_DC] = {
        .name = "DC",
        .samples = _dc_table,
        .length = 1,
        .min_value = 4095,
        .max_value = 4095,
    },
    [WAVEFORM_BURST] = {
        .name = "Burst",
        .samples = _burst_table,
        .length = TABLE_LENGTH,
        .min_value = 0,
        .max_value = 8191,
    },
    [WAVEFORM_STAIRCASE] = {
        .name = "Stairs",
        .samples = _stairs_table,
        .length = TABLE_LENGTH,
        .min_value = 0,
        .max_value = 8191,
    },
};

const waveform_definition_t *waveforms_get_definition(waveform_id_t id)
{
    if (id >= WAVEFORM_COUNT) {
        return NULL;
    }
    return &_waveforms[id];
}

const waveform_definition_t *waveforms_get_all(size_t *count)
{
    if (count) {
        *count = WAVEFORM_COUNT;
    }
    return _waveforms;
}

uint16_t waveforms_sample_scaled(const waveform_definition_t *waveform, size_t index, uint16_t target_max)
{
    assert(waveform);
    if (!waveform->samples || waveform->length == 0) {
        return 0;
    }
    uint16_t value = waveform->samples[index % waveform->length];
    if (waveform->max_value == waveform->min_value) {
        return target_max / 2;
    }
    uint32_t scaled = (uint32_t)(value - waveform->min_value) * target_max;
    scaled /= (waveform->max_value - waveform->min_value);
    return (uint16_t)scaled;
}

