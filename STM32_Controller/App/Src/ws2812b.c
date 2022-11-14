// SPDX-License-Identifier: GPL-2.0-only
/*
 * ws2812b.c - STM32 library for controlling LED strips based on
 * 			   WS2812B IC using SPI interface
 *  (C) 2022 Pawel Wieczorek
 */

#include "main.h"
#include "ws2812.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/************************
 * PRIVATE MACROS
 ************************/
#define SPI_DATA_0				0b001
#define SPI_DATA_1				0b011
#define SPI_BYTES_PER_DIODE		9
#define SPI_MAX_DELAY			-1
#define SPI_RES_SIGNAL_LEN		125

#define SPI_BUF_LEN(LEDS)		((LEDS * SPI_BYTES_PER_DIODE) + SPI_RES_SIGNAL_LEN)


/************************
 * EXPORTED GLOBALS
 ************************/
struct LEDStrip* led_strip_upper;
struct LEDStrip* led_strip_bottom;

/************************
 * PRIVATE FUNCTIONS
 ************************/
static uint32_t encode_byte(uint8_t byte) {
	uint8_t bitmask = 1;
	uint32_t data = 0;

	for(int i = 7; i >= 0; i--) {
		data <<= 3;
		if(byte & bitmask)
			data |= SPI_DATA_1;
		else
			data |= SPI_DATA_0;
		bitmask <<= 1;
	}

	return data;
}

static void clear_buffer(uint8_t* buffer, uint32_t len) {
	uint32_t pattern = encode_byte(0x00);

	for(uint32_t i = 0; i < len; i += 3)
		memcpy(&buffer[i], &pattern, 3);
}

/************************
 * EXPORTED FUNCTIONS
 ************************/
int init_led_strip(struct LEDStrip** stripp, SPI_HandleTypeDef* hspi, uint32_t len) {
	struct LEDStrip* strip;

	*stripp = malloc(sizeof(struct LEDStrip));
	if(*stripp == NULL)
		return -1;
	strip = *stripp;

	strip->spi = hspi;
	strip->len = len;
	strip->brightness = 255;
	strip->apply_ratio = 0;
	strip->_data_buffer = malloc(SPI_BUF_LEN(len));
	if(strip->_data_buffer == NULL)
		return -1;
	clear_buffer(strip->_data_buffer, len * SPI_BYTES_PER_DIODE);
	memset(strip->_data_buffer + len * SPI_BYTES_PER_DIODE, 0, SPI_RES_SIGNAL_LEN);

	return 0;
}

int get_leds_count(struct LEDStrip* strip) {
	return strip->len;
}

void set_led_color(struct LEDStrip* strip, int idx, struct Color color) {
	uint32_t data;
	uint32_t offset = idx * SPI_BYTES_PER_DIODE;

	if(idx >= strip->len) {
		printk(LOG_ERR "WS2812B: set_led_color invoked with invalid idx (%d) "
				"; strip len (%d)", idx, strip->len);
		return;
	}

	// Apply brightness to selected color
	color.r = ((uint16_t) color.r * strip->brightness) / 255;
	color.g = ((uint16_t) color.g * strip->brightness) / 255;
	color.b = ((uint16_t) color.b * strip->brightness) / 255;

	// Apply color calibration data
	if(strip->apply_ratio) {
		if(idx < strip->ratio_index) {
			color.r = ((uint32_t) color.r * strip->rt1_r.num) / strip->rt1_r.denum;
			color.g = ((uint32_t) color.g * strip->rt1_g.num) / strip->rt1_g.denum;
			color.b = ((uint32_t) color.b * strip->rt1_b.num) / strip->rt1_b.denum;
		} else {
			color.r = ((uint32_t) color.r * strip->rt2_r.num) / strip->rt2_r.denum;
			color.g = ((uint32_t) color.g * strip->rt2_g.num) / strip->rt2_g.denum;
			color.b = ((uint32_t) color.b * strip->rt2_b.num) / strip->rt2_b.denum;
		}
	}

	data = encode_byte(color.g);
	memcpy(&strip->_data_buffer[offset], &data, 3);
	data = encode_byte(color.r);
	memcpy(&strip->_data_buffer[offset + 3], &data, 3);
	data = encode_byte(color.b);
	memcpy(&strip->_data_buffer[offset + 6], &data, 3);
}

void refresh_leds(struct LEDStrip* strip) {
	int ret;

	// Await for DMA to be ready
	while(HAL_SPI_GetState(strip->spi) != HAL_SPI_STATE_READY) {}

	ret = HAL_SPI_Transmit_DMA(strip->spi, strip->_data_buffer, SPI_BUF_LEN(strip->len));
	if(ret != HAL_OK) {
		printk(LOG_ERR "WS2812B: HAL_SPI transmit returned with an error - %d\n", ret);
	}
}

void clear_leds(struct LEDStrip* strip) {
	clear_buffer(strip->_data_buffer, strip->len * SPI_BYTES_PER_DIODE);
}

void set_leds_brightness(struct LEDStrip* strip, uint8_t brightness) {
	strip->brightness = brightness;
}

void calibrate_leds_colors(struct LEDStrip* strip, struct Ratio r, struct Ratio g, struct Ratio b, uint16_t idx) {
	if(idx == 0) {
		strip->rt1_r = r;
		strip->rt1_g = g;
		strip->rt1_b = b;
		strip->ratio_index = -1;
	} else {
		strip->rt2_r = r;
		strip->rt2_g = g;
		strip->rt2_b = b;
		strip->ratio_index = idx;
	}
	strip->apply_ratio = 1;
}

struct Color int2Color(int color) {
	struct Color c = {
			.r = color & 0xff,
			.g = color >> 8,
			.b = color >> 16,
	};
	return c;
}
