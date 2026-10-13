/* SPDX-License-Identifier: MIT */
#ifndef DVB_DTS_FRAME_H
#define DVB_DTS_FRAME_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Normalize the four DTS core wire formats to the Dream decoder's 16BE input. */
static inline size_t dvb_dts_normalize(uint8_t *output, size_t capacity,
	const uint8_t *input, size_t size)
{
	uint32_t sync, bits = 0;
	unsigned int bit_count = 0;
	size_t written = 0;
	int little_endian, packed;

	if (!output || !input || size < 4 || capacity < size)
		return 0;
	sync = (uint32_t)input[0] << 24 | (uint32_t)input[1] << 16 |
		(uint32_t)input[2] << 8 | input[3];
	if (sync == UINT32_C(0x7ffe8001))
	{
		memcpy(output, input, size);
		return size;
	}
	little_endian = sync == UINT32_C(0xfe7f0180) || sync == UINT32_C(0xff1f00e8);
	packed = sync == UINT32_C(0x1fffe800) || sync == UINT32_C(0xff1f00e8);
	if ((!little_endian && !packed) || (size & 1))
		return 0;
	for (size_t i = 0; i < size; i += 2)
	{
		uint32_t word = little_endian ? (uint32_t)input[i + 1] << 8 | input[i]
			: (uint32_t)input[i] << 8 | input[i + 1];
		if (!packed)
		{
			output[written++] = (uint8_t)(word >> 8);
			output[written++] = (uint8_t)word;
			continue;
		}
		bits = (bits << 14) | (word & 0x3fff);
		bit_count += 14;
		while (bit_count >= 8)
		{
			bit_count -= 8;
			output[written++] = (uint8_t)(bits >> bit_count);
		}
	}
	return written;
}

#endif
