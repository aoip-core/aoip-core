#include "aoip/tone.h"

void float_to_l24(float_t f, int32_t buf[]) {
	l24_t l24;
	l24_t *b = (l24_t *)buf;

	if (f >= 1.0) {
		l24.i32 = 2147483647;
	} else if (f <= -1.0) {
		l24.i32 = -2147483648;
	} else {
		l24.i32 = f * 2147483648.0;
	}
	b->u8[3] = l24.u8[1];
	b->u8[2] = l24.u8[2];
	b->u8[1] = l24.u8[3];
	b->u8[0] = 0;
}

int32_t float_to_i32(float_t tonef) {
	int32_t tone_i32;

	if (tonef >= 1.0) {
		tone_i32 = 2147483647;
	} else if (tonef <= -1.0) {
		tone_i32 = -2147483648;
	} else {
		tone_i32 = tonef * 2147483648.0;
	}

	return tone_i32;
}
