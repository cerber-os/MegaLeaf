// SPDX-License-Identifier: GPL-2.0-only
/*
 * ws2812b.c - STM32 library for controlling LED strips based on
 * 			   WS2812B IC using SPI interface
 *  (C) 2022 Pawel Wieczorek
 */

#include "main.h"
#include "ws2812.h"

#include <stdint.h>
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

#define SPI_
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

	assert(idx < strip->len);

	// Apply brightness to selected color
	color.r = (uint8_t)(color.r * (float)strip->brightness / 255.0);
	color.g = (uint8_t)(color.g * (float)strip->brightness / 255.0);
	color.b = (uint8_t)(color.b * (float)strip->brightness / 255.0);

	data = encode_byte(color.g);
	memcpy(&strip->_data_buffer[offset], &data, 3);
	data = encode_byte(color.r);
	memcpy(&strip->_data_buffer[offset + 3], &data, 3);
	data = encode_byte(color.b);
	memcpy(&strip->_data_buffer[offset + 6], &data, 3);
}

void refresh_leds(struct LEDStrip* strip) {
	int ret;

	__disable_irq();
	ret = HAL_SPI_Transmit(strip->spi, strip->_data_buffer, SPI_BUF_LEN(strip->len), SPI_MAX_DELAY);
	__enable_irq();

	if(ret != HAL_OK) {
		printf("|%s| HAL_SPI_Transmit returned with an error - %d\n", __func__, ret);
	}
}

void clear_leds(struct LEDStrip* strip) {
	clear_buffer(strip->_data_buffer, strip->len * SPI_BYTES_PER_DIODE);
}

void set_leds_brightness(struct LEDStrip* strip, uint8_t brightness) {
	strip->brightness = brightness;
}

struct Color int2Color(int color) {
	struct Color c = {
			.r = color & 0xff,
			.g = color >> 8,
			.b = color >> 16,
	};
	return c;
}
