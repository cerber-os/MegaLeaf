// SPDX-License-Identifier: GPL-2.0-only
/*
 * mlf_effects.c - implementation of awesome effect that can be displayed on panel
 *  (C) 2021 Pawel Wieczorek
 */

#include "mlf_effects.h"

#include "ws2812.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/***************************
 * UTILS
 ***************************/
/*
 * HSL2RGB code adapted from:
 *  https://stackoverflow.com/a/9493060
 */
static float hue2rgb(float p, float q, float t) {
  if (t < 0) t += 1;
  if (t > 1) t -= 1;
  if (t < 1./6) return p + (q - p) * 6 * t;
  if (t < 1./2) return q;
  if (t < 2./3) return p + (q - p) * (2./3 - t) * 6;
  return p;
}

static struct Color hsl2rgb(float h, float s, float l) {
  struct Color result;

  if(0 == s)
    result.r = result.g = result.b = l;
  else {
    float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    float p = 2 * l - q;
    result.r = round(hue2rgb(p, q, h + 1./3) * 255);
    result.g = round(hue2rgb(p, q, h) * 255);
    result.b = round(hue2rgb(p, q, h - 1./3) * 255);
  }

  return result;
}

/***************************
 * RGB EFFECTS IMPLEMENTATION
 ***************************/
static int effect_rainbow(struct LEDStrip* strip, uint32_t frame, uint32_t data) {
	int i;
	float partialHue;
	float hue = 0.001 * frame;
	const int leds_count = get_leds_count(strip);

	// Convert hue to range [0,1]
	hue = hue - (long)hue;

	for(i = 0; i < leds_count; i++) {
		partialHue = hue + (float)i / leds_count;
		if(partialHue > 1.0f)
		  partialHue -= 1.0f;

		set_led_color(strip, i, hsl2rgb(partialHue, 1, 0.5));
	}

	return MLF_EFFECT_REFRESH_REQ;
}

static int effect_color_cycle(struct LEDStrip* strip, uint32_t frame, uint32_t data) {
	int i;
	float hue = 0.001 * frame;
	const int leds_count = get_leds_count(strip);
	struct Color color;

	// Convert hue to range [0,1]
	hue = hue - (long)hue;
	color = hsl2rgb(hue, 1, 0.5);

	for(i = 0; i < leds_count; i++)
		set_led_color(strip, i, color);

	return MLF_EFFECT_REFRESH_REQ;
}

static int effect_static_color(struct LEDStrip* strip, uint32_t frame, uint32_t data) {
	int i;
	const int leds_count = get_leds_count(strip);

	struct Color color;
	color.r = data & 0xff;
	color.g = (data >> 8) & 0xff;
	color.b = (data >> 16) & 0xff;

	for(i = 0; i < leds_count; i++)
		set_led_color(strip, i, color);

	return 0;
}

static int effect_bar_cycle(struct LEDStrip* strip, uint32_t frame, uint32_t data) {
	int i, width, pos;
	const int leds_count = get_leds_count(strip);
	struct Color color, target;

	color.r = data & 0xff;
	color.g = (data >> 8) & 0xff;
	color.b = (data >> 16) & 0xff;

	pos = frame % 100;
	pos /= leds_count;
	width = leds_count / 5;

	for(i = 0; i < pos; i++)
		set_led_color(strip, i, (struct Color) {0, 0, 0});
	for(i = pos; i < pos + width; i++) {
		target.r = color.r;
		target.g = color.g;
		target.b = color.b;
		set_led_color(strip, i % leds_count, target);
	}
	for(i = pos + width; i < leds_count; i++)
		set_led_color(strip, i, (struct Color) {0, 0, 0});

	return MLF_EFFECT_REFRESH_REQ;
}

/***************************
 * HANDLERS FOR SUPPORTED EFFECTS
 ***************************/
static MLF_effects_handlers effect_handlers[EFFECT_MAX] = {
		[EFFECT_RAINBOW] = 		effect_rainbow,
		[EFFECT_COLOR_CYCLE] = 	effect_color_cycle,
		[EFFECT_STATIC_COLOR] = effect_static_color,
		[EFFECT_BAR_CYCLE] = effect_bar_cycle,
};


int run_effect_frame(struct LEDStrip* strip, enum MLF_EFFECTS effect, uint32_t frame, uint32_t data) {
	if(effect < 0 || effect >= EFFECT_MAX)
		return -1;
	if(effect_handlers[effect] == NULL)
		return -1;

	return effect_handlers[effect](strip, frame, data);
}

