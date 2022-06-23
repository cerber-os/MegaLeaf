// SPDX-License-Identifier: GPL-2.0-only
/*
 * app.c - main application routine
 *  (C) 2022 Pawel Wieczorek
 */

#include "app.h"
#include "ws2812.h"
#include "mlf_protocol.h"
#include "mlf_effects.h"

#include "stm32l5xx_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***********************
 * REMOTE GLOBALS
 ***********************/
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi3;


/***********************
 * COMMANDS HANDLERS
 ***********************/
enum APP_OP_MODE {
	TURN_OFF    = 0,
	SHOW_EFFECT,
	SHOW_COLORS,

	MAX_OP_MODE,
} app_mode;

enum MLF_EFFECTS cur_effect_top = 0;
enum MLF_EFFECTS cur_effect_bottom = 0;
uint32_t cur_effect_speed = 1;
uint32_t cur_effect_top_data, cur_effect_bottom_data;

int app_get_info(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	struct MLF_resp_cmd_get_info info = {
			.fw_version = 1,
			.leds_count_top = get_leds_count(led_strip_upper),
			.leds_count_bottom = get_leds_count(led_strip_bottom),
	};

	memcpy(resp, &info, sizeof info);
	*resp_len = sizeof info;
	return MLF_RET_OK;
}

int app_turn_off(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	app_mode = TURN_OFF;
	return MLF_RET_OK;
}

int app_set_brightness(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	struct MLF_req_cmd_set_brightness* cmd_data = NULL;

	if(len < sizeof(*cmd_data))
		return MLF_RET_INVALID_DATA;

	cmd_data = (struct MLF_req_cmd_set_brightness*) data;
	if(cmd_data->strip & STRIP_TOP)
		set_leds_brightness(led_strip_upper, cmd_data->brightness);
	if(cmd_data->strip & STRIP_BOTTOM)
		set_leds_brightness(led_strip_bottom, cmd_data->brightness);

	return MLF_RET_OK;
}

int app_set_effect(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	struct MLF_req_cmd_set_effect* cmd_data = NULL;

	if(len < sizeof(*cmd_data))
		return MLF_RET_INVALID_DATA;

	cmd_data = (struct MLF_req_cmd_set_effect*) data;
	if(cmd_data->effect >= EFFECT_MAX)
		return MLF_RET_INVALID_DATA;

	cur_effect_speed = cmd_data->speed + 1;
	if(cmd_data->strip & STRIP_TOP) {
		cur_effect_top = cmd_data->effect;
		cur_effect_top_data = cmd_data->color;
	}
	if(cmd_data->strip & STRIP_BOTTOM) {
		cur_effect_bottom = cmd_data->effect;
		cur_effect_bottom_data = cmd_data->color;
	}

	app_mode = SHOW_EFFECT;
	return MLF_RET_OK;
}

int app_set_color(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	int* leds = (int*)(data + sizeof(struct MLF_req_cmd_set_color));
	int upperLedsCnt, bottomLedsCnt;
	struct MLF_req_cmd_set_color* cmd_data = (struct MLF_req_cmd_set_color*) data;

	if(len < sizeof(*cmd_data))
		return MLF_RET_INVALID_DATA;
	len -= sizeof(*cmd_data);

	upperLedsCnt = get_leds_count(led_strip_upper);
	bottomLedsCnt = get_leds_count(led_strip_bottom);

	for(int i = 0; i < len / 4; i++) {
		if(i >= bottomLedsCnt + upperLedsCnt)
			break;
		else if(i >= bottomLedsCnt)
			set_led_color(led_strip_upper, i - bottomLedsCnt, int2Color(leds[i]));
		else
			set_led_color(led_strip_bottom, i, int2Color(leds[i]));
	}

	app_mode = SHOW_COLORS;
	return MLF_RET_OK;
}

/***********************
 * EXPORTED FUNCTIONS
 ***********************/
void app_init(void) {
	printf("\n\n*********** RESET ***********\n\n");

	MLF_init();

	printf("Initialising LED strip\n");
	init_led_strip(&led_strip_bottom, &hspi1, 216);
	init_led_strip(&led_strip_upper, &hspi3, 90);

	refresh_leds(led_strip_bottom);
	refresh_leds(led_strip_upper);

	// Register USB callbacks
	MLF_register_callback(MLF_CMD_GET_INFO, app_get_info);
	MLF_register_callback(MLF_CMD_TURN_OFF, app_turn_off);
	MLF_register_callback(MLF_CMD_SET_EFFECT, app_set_effect);
	MLF_register_callback(MLF_CMD_SET_BRIGHTNESS, app_set_brightness);
	MLF_register_callback(MLF_CMD_SET_COLOR, app_set_color);
}


void app_main_loop(void) {
	uint32_t tick_start;
	uint8_t refresh = 1;
	uint32_t frame = 0;

	while(1) {
		// Wait at least 10ms or shorter if packet arrives
		tick_start = HAL_GetTick();
		while(HAL_GetTick() - tick_start < 15) {
			if(MLF_is_packet_available()) {
				MLF_process_packet();

				// Always refresh LEDs state upon receiving new packet
				refresh = 1;
				break;
			}
		}

		switch(app_mode) {
		case TURN_OFF:
			if(refresh) {
				clear_leds(led_strip_bottom);
				clear_leds(led_strip_upper);
			}
			break;
		case SHOW_EFFECT:
			refresh |= run_effect_frame(led_strip_upper, cur_effect_top, frame, cur_effect_top_data);
			refresh |= run_effect_frame(led_strip_bottom, cur_effect_bottom, frame, cur_effect_bottom_data);
			frame += cur_effect_speed;
			break;
		case SHOW_COLORS:
			// Everything is already done by a handler
			break;

		default:
			puts("How the hell we end up here?");
			break;
		}

		// Reconfigure LEDs signal only when required
		if(refresh) {
			refresh_leds(led_strip_bottom);
			refresh_leds(led_strip_upper);
		}
		refresh = 0;
	}
}
