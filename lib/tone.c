#include "aoip/tone.h"

void float_to_l24(uint8_t l24[], float_t f) {
	int32_t tmp_i32 = 0;
	uint8_t *tmp_ptr = (uint8_t *)&tmp_i32;

	if (f >= 1.0) {
		tmp_i32 = 2147483647;
	} else if (f <= -1.0) {
		tmp_i32 = -2147483648;
	} else {
		tmp_i32 = floor(f * 2147483648.0);
	}

	l24[2] = tmp_ptr[0];
	l24[1] = tmp_ptr[1];
	l24[0] = tmp_ptr[2];
}
