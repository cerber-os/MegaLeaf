// SPDX-License-Identifier: GPL-2.0-only
/*
 * app.c - main application routine
 *  (C) 2022 Pawel Wieczorek
 */

#include "app.h"
#include "ws2812.h"
#include "mlf_protocol.h"
#include "mlf_effects.h"
#include "logger.h"
#include "panic.h"

#include "stm32f4xx_hal.h"
#include "usbd_cdc_if.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***********************
 * REMOTE GLOBALS
 ***********************/
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart2;
extern IWDG_HandleTypeDef hiwdg;


/***********************
 * GLOBALS
 ***********************/
struct MLF_ctx usb_ctx;
struct MLF_ctx usart_ctx;
struct packet_buffer* usart_packet_buf;

/***********************
 * COMMANDS HANDLERS
 ***********************/
enum APP_OP_MODE {
	TURN_OFF    = 0,
	SHOW_EFFECT,
	SHOW_COLORS,

	MAX_OP_MODE,
} app_mode, old_app_mode;

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

int app_turn_on(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	app_mode = old_app_mode;
	return MLF_RET_OK;
}

int app_turn_off(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	if(app_mode != TURN_OFF) {
		old_app_mode = app_mode;
		app_mode = TURN_OFF;
	}
	return MLF_RET_OK;
}

int app_set_brightness(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	struct MLF_req_cmd_set_brightness* cmd_data = NULL;

	if(len < sizeof(*cmd_data)) {
		printk(LOG_ERR "app: set brightness command got incorrect len(%d)", len);
		return MLF_RET_INVALID_DATA;
	}

	cmd_data = (struct MLF_req_cmd_set_brightness*) data;
	if(cmd_data->strip & STRIP_TOP)
		set_leds_brightness(led_strip_upper, cmd_data->brightness);
	if(cmd_data->strip & STRIP_BOTTOM)
		set_leds_brightness(led_strip_bottom, cmd_data->brightness);

	return MLF_RET_OK;
}

int app_set_effect(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	struct MLF_req_cmd_set_effect* cmd_data = NULL;

	if(len < sizeof(*cmd_data)) {
		printk(LOG_ERR "app: set effect command got incorrect len(%d)", len);
		return MLF_RET_INVALID_DATA;
	}

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

static int app_get_brightness(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	struct MLF_resp_cmd_get_brightness bright;

	bright.brightness = led_strip_bottom->brightness / 2 + led_strip_upper->brightness / 2;

	memcpy(resp, &bright, sizeof bright);
	*resp_len = sizeof bright;
	return MLF_RET_OK;
}

static int app_get_effect(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	// TODO: Send state of both strips somehow
	struct MLF_resp_cmd_get_effect effect = {
			.effect = cur_effect_top,
			.speed = cur_effect_speed,
			.color = cur_effect_top_data,
	};

	memcpy(resp, &effect, sizeof effect);
	*resp_len = sizeof effect;
	return MLF_RET_OK;
}

static int app_get_on_state(uint8_t* data, uint16_t len, uint8_t* resp, uint16_t* resp_len) {
	struct MLF_resp_cmd_get_on_state mode = {
			.is_on = app_mode != TURN_OFF,
	};

	memcpy(resp, &mode, sizeof mode);
	*resp_len = sizeof mode;
	return MLF_RET_OK;
}

static int USB_CDC_Transmit_FS(uint8_t* buf, uint16_t size) {
	int ret = CDC_Transmit_FS(buf, size);
	switch(ret) {
	case USBD_OK:
		return HAL_OK;
	case USBD_BUSY:
		return HAL_BUSY;
	default:
		return HAL_ERROR;
	}
}

/***********************
 * IRQ-safe buffer
 *  We assume there's only ONE writer and ONE reader
 ***********************/
#define IRQ_BUFFER_SIZE	64
static struct IRQ_buffer {
	uint16_t size;
	uint8_t  buf[IRQ_BUFFER_SIZE];
} USART2_IRQ_buffer = {
		.size = 0
};

void IRQ_buffer_push(uint8_t element) {
	if(USART2_IRQ_buffer.size >= IRQ_BUFFER_SIZE) {
		printk(LOG_ERR "usart2: got USART buffer overflow");
		return;
	}

	USART2_IRQ_buffer.buf[USART2_IRQ_buffer.size] = element;
	USART2_IRQ_buffer.size++;
}

int IRQ_buffer_pop(uint8_t* data, uint16_t* size) {
	int ret = 0, to_copy = 0;

	__disable_irq();

	if(USART2_IRQ_buffer.size == 0) {
		ret = -1;
		goto exit;
	}

	to_copy = (*size < USART2_IRQ_buffer.size) ? *size : USART2_IRQ_buffer.size;
	memcpy(data, USART2_IRQ_buffer.buf, to_copy);
	// Move remaining data to the front
	memcpy(USART2_IRQ_buffer.buf, USART2_IRQ_buffer.buf + to_copy, USART2_IRQ_buffer.size - to_copy);
	USART2_IRQ_buffer.size -= to_copy;

exit:
	__enable_irq();
	*size = to_copy;
	return ret;
}

/***********************
 * USART2 MLF SUPPORT
 ***********************/
void USART2_IRQHandler(void) {
	uint8_t c;
	if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
		c = huart2.Instance->DR;
		IRQ_buffer_push(c);
	} else if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
		c = huart2.Instance->DR;
		printk(LOG_ERR "%s: got USART overflow", __func__);
	}
}

int USART2_WriteData(uint8_t* data, uint16_t size) {
	return HAL_UART_Transmit(&huart2, data, size, 1000);
}

/***********************
 * PRIVATE FUNCTIONS
 ***********************/
static void register_default_callback(struct MLF_ctx* ctx) {
	MLF_register_callback(ctx, MLF_CMD_GET_INFO, app_get_info);
	MLF_register_callback(ctx, MLF_CMD_TURN_ON, app_turn_on);
	MLF_register_callback(ctx, MLF_CMD_TURN_OFF, app_turn_off);
	MLF_register_callback(ctx, MLF_CMD_SET_EFFECT, app_set_effect);
	MLF_register_callback(ctx, MLF_CMD_SET_BRIGHTNESS, app_set_brightness);
	MLF_register_callback(ctx, MLF_CMD_SET_COLOR, app_set_color);
	MLF_register_callback(ctx, MLF_CMD_GET_BRIGHTNESS, app_get_brightness);
	MLF_register_callback(ctx, MLF_CMD_GET_EFFECT, app_get_effect);
	MLF_register_callback(ctx, MLF_CMD_GET_ON_STATE, app_get_on_state);
}

/***********************
 * EXPORTED FUNCTIONS
 ***********************/
void app_init(void) {
	// Setup MLF protocol for USB port
	MLF_init(&usb_ctx, USB_CDC_Transmit_FS);

	// Setup MLF protocol for internal USART2
	MLF_init(&usart_ctx, USART2_WriteData);
	usart_packet_buf = packet_buffer_init(&usart_ctx);
	__HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);

	printk(LOG_INFO "app: Initialising LED strip");
	init_led_strip(&led_strip_bottom, &hspi2, 216);
	init_led_strip(&led_strip_upper, &hspi1, 90);

	calibrate_leds_colors(led_strip_bottom,
			(struct Ratio){1, 1},		// red - no change
			(struct Ratio){0x60, 0xa0},	// green - 0x60 -> 0xa0
			(struct Ratio){1, 1},		// blue - no change
			0 /* From first LED in strip */);
	calibrate_leds_colors(led_strip_bottom,
				(struct Ratio){1, 1},		// red - no change
				(struct Ratio){0x80, 0xa0},	// green - 0x80 -> 0xa0
				(struct Ratio){1, 1},		// blue - no change
				144 /* From 144th LED in strip */);
	refresh_leds(led_strip_bottom);
	refresh_leds(led_strip_upper);

	register_default_callback(&usb_ctx);
	register_default_callback(&usart_ctx);

	MLF_register_reroute(&usb_ctx, &usart_ctx, MLF_CMD_SET_BRIGHTNESS);
	MLF_register_reroute(&usb_ctx, &usart_ctx, MLF_CMD_SET_EFFECT);
	MLF_register_reroute(&usb_ctx, &usart_ctx, MLF_CMD_TURN_OFF);
	MLF_register_reroute(&usb_ctx, &usart_ctx, MLF_CMD_TURN_ON);
}


void app_main_loop(void) {
	uint8_t data_buffer[64];
	uint32_t tick_start;
	uint8_t refresh = 1;
	uint32_t frame = 0;

	printk(LOG_INFO "app: Starting main loop");

	while(1) {
		// Pet watchdog
		HAL_IWDG_Refresh(&hiwdg);

		// Wait at least 10ms or shorter if packet arrives
		tick_start = HAL_GetTick();
		while(HAL_GetTick() - tick_start < 15) {
			// Handle new bytes in USART2 IRQ queue
			int ret;
			uint16_t size = sizeof(data_buffer);
			ret = IRQ_buffer_pop(data_buffer, &size);
			if(ret >= 0)
				packet_buffer_append(usart_packet_buf, data_buffer, size);

			// Check for new packets
			if(MLF_is_packet_available(&usb_ctx)) {
				MLF_process_packet(&usb_ctx);

				// Always refresh LEDs state upon receiving new packet
				refresh = 1;
				break;
			} else if(MLF_is_packet_available(&usart_ctx)) {
				MLF_process_packet(&usart_ctx);
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
			panic("Unexpected APP_MODE has been selected");
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
