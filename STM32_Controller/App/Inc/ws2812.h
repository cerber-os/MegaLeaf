// SPDX-License-Identifier: GPL-2.0-only
/*
 * ws2812.h - STM32 library for controlling LED strips based on
 * 			  WS2812B IC using SPI interface
 *  (C) 2022 Pawel Wieczorek
 */
#ifndef INC_WS2812_H_
#define INC_WS2812_H_

#include <stm32l5xx_hal.h>

/*
 * Exported structures
 */
struct Color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} __packed;

struct LEDStrip {
	SPI_HandleTypeDef* spi;
	uint32_t len;
	uint8_t* _data_buffer;
	uint8_t brightness;
};

/*
 * Exported globals
 */
extern struct LEDStrip* led_strip_upper;
extern struct LEDStrip* led_strip_bottom;

/*
 * Exported functions
 */
int init_led_strip(struct LEDStrip** stripp, SPI_HandleTypeDef* hspi, uint32_t len);
int get_leds_count(struct LEDStrip* strip);
void set_led_color(struct LEDStrip* strip, int idx, struct Color color);
void refresh_leds(struct LEDStrip* strip);
void clear_leds(struct LEDStrip* strip);
void set_leds_brightness(struct LEDStrip* strip, uint8_t brightness);

#endif /* INC_WS2812_H_ */
