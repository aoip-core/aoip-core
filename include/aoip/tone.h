#pragma once

#include <math.h>
#include <inttypes.h>

#define TWO_PI (2.0f * M_PI)

#define TONE_FREQ_C4 261.6f
#define TONE_FREQ_D4 293.7f
#define TONE_FREQ_E4 329.6f
#define TONE_FREQ_F4 349.2f
#define TONE_FREQ_G4 392.0f
#define TONE_FREQ_A4 440.0f
#define TONE_FREQ_B4 493.9f
#define TONE_FREQ_C5 523.3f

typedef union {
	int32_t i32;
	uint8_t u8[4];
} l24_t;

static inline float_t generate_tone_data(float_t period) {
       return sinf(TWO_PI * period);
}

void float_to_l24(float_t, int32_t *);
