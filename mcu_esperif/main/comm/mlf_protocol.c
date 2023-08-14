// SPDX-License-Identifier: GPL-2.0-only
/*
 * mlf_protocol.c - 
 *  (C) 2021 Pawel Wieczorek
 */

#include "mlf_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void MLF_resp_error(struct MLF_ctx* ctx, enum MLF_error_codes error);
static int MLF_validate_header(struct MLF_ctx* ctx, uint8_t* buf);
static int MLF_validate_footer(struct MLF_ctx* ctx, uint8_t* buf);
static void MLF_submit_packet(struct MLF_ctx* ctx, uint8_t* buf, uint32_t len);

/**********************
 * COMPATIBILITY LAYER
 **********************/
#ifdef USE_HAL_DRIVER
#include "stm32l5xx_hal.h"
#elif defined(ESP_PLATFORM)

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define HAL_OK		0
#define HAL_BUSY	2

static void HAL_Delay(uint16_t ms) {
	vTaskDelay(10 / portTICK_PERIOD_MS);
}

uint32_t HAL_GetTick(void) {
	return xTaskGetTickCount();
}

#else
#error "Unsupported platform"
#endif

/**********************
 * LOGGING SUPPORT
 **********************/
#define LOG_INFO(MSG, ...)		printf("[INFO] |%s| " MSG "\n", __func__, ##__VA_ARGS__);
#define LOG_WARN(MSG, ...)		printf("[WARN] |%s| " MSG "\n", __func__, ##__VA_ARGS__);
#define LOG_ERROR(MSG, ...)		printf("[ERROR]|%s| " MSG "\n", __func__, ##__VA_ARGS__);

/**********************
 * PACKET BUFFER SUPPORT
 *  - for processing data received from client, we need
 *    simple buffer to which data can be appended and which
 *    will drop partial transfers
 **********************/
#define PACKET_BUFFER_MAX_DELAY		1000
#define PACKET_BUFFER_MAX_SIZE		(MLF_MAX_DATA_SIZE + \
										sizeof(struct MLF_req_packet_header) + \
										sizeof(struct MLF_packet_footer))

struct packet_buffer {
	struct MLF_ctx* ctx;

	uint8_t  header_validated;
	uint32_t time_last_packet;
	uint32_t size;
	uint8_t  buffer[PACKET_BUFFER_MAX_SIZE];
};


static void packet_buffer_clear(struct packet_buffer* pkt) {
	pkt->size = 0;
	pkt->time_last_packet = 0;
	pkt->header_validated = 0;
	memset(pkt->buffer, 0, sizeof pkt->buffer);
}

struct packet_buffer* packet_buffer_init(struct MLF_ctx* ctx) {
	struct packet_buffer* pkt;

	pkt = malloc(sizeof *pkt);
	if(pkt == NULL) {
		LOG_ERROR("Failed to allocate memory for new packet_buffer");
		return NULL;
	}

	pkt->ctx = ctx;
	packet_buffer_clear(pkt);
	return pkt;
}

void packet_buffer_deinit(struct packet_buffer* pkt) {
	free(pkt);
}

static int packet_buffer_overtime(struct packet_buffer* pkt) {
	return pkt->time_last_packet && (HAL_GetTick() - pkt->time_last_packet >= PACKET_BUFFER_MAX_DELAY);
}

int packet_buffer_append(struct packet_buffer* pkt, uint8_t* buf, uint32_t len) {
	int ret;
	uint32_t footer_offset;

	if(len == 0)
		return 0;

	if(packet_buffer_overtime(pkt)) {
		LOG_WARN("Packet timeout. Treating data as a new packet");
		packet_buffer_clear(pkt);
	}

	if(pkt->size + len >= PACKET_BUFFER_MAX_SIZE) {
		LOG_ERROR("Packet buffer overflow");
		packet_buffer_clear(pkt);
		MLF_resp_error(pkt->ctx, MLF_RET_DATA_TOO_LARGE);
		return -1;
	}

	// Append new data to our packed buffer
	memcpy(pkt->buffer + pkt->size, buf, len);
	pkt->size += len;

	// Validate header and footer of received data
	if(!pkt->header_validated && pkt->size >= sizeof(struct MLF_req_packet_header)) {
		ret = MLF_validate_header(pkt->ctx, pkt->buffer);
		if(ret) {
			// MLF_validate_header already reports an error to host
			packet_buffer_clear(pkt);
			return -1;
		}
		pkt->header_validated = 1;
	}

	footer_offset = sizeof(struct MLF_req_packet_header) +
					((struct MLF_req_packet_header*)pkt->buffer)->data_size;
	if(pkt->size >= footer_offset + sizeof(struct MLF_packet_footer)) {
		ret = MLF_validate_footer(pkt->ctx, pkt->buffer);
		if(ret) {
			// MLF_validate_footer already reports an error to host
			packet_buffer_clear(pkt);
			return -1;
		}

		// Full packet has been received
		MLF_submit_packet(pkt->ctx, pkt->buffer, pkt->size);
		packet_buffer_clear(pkt);
		return 0;
	}

	pkt->time_last_packet = HAL_GetTick();
	return 0;
}


/**********************
 * MegaLeaf (MLF) Packet Support
 **********************/
#define MAX_RESPONSE_DELAY		6

int MLF_init(struct MLF_ctx* ctx, MLF_write_func write_func) {
	memset(ctx, 0, sizeof(*ctx));

	// TODO: MLF_deinit + free this array!
	ctx->recv_packet_buf = malloc(PACKET_BUFFER_MAX_SIZE);
	if(ctx->recv_packet_buf == NULL)
		return -1;
	memset(ctx->recv_packet_buf, 0, PACKET_BUFFER_MAX_SIZE);
	ctx->write_func = write_func;

	return 0;
}

static void MLF_resp_error(struct MLF_ctx* ctx, enum MLF_error_codes error) {
	int ret = 0;
	struct MLF_resp_packet_header header = {
			.magic = MLF_RESP_HEADER_MAGIC,
			.error_code = error,
			.data_size = 0
	};
	struct MLF_packet_footer footer = {
			.magic = MLF_FOOTER_MAGIC
	};

	uint8_t output_buffer[sizeof header + sizeof footer];
	memcpy(output_buffer, &header, sizeof(header));
	memcpy(output_buffer + sizeof header, &footer, sizeof(footer));

	ret = ctx->write_func(output_buffer, sizeof output_buffer);
	if(ret)
		LOG_ERROR("Failed to send error reponse - write_func returned %d", ret);
		// We might be in interrupt context here, so there's literally nth we could do about
		//  this error. Normally, if it's USB_BUSY error, we would sleep a while, but
		//  here, it's better to just give up
		// TODO: Check if we're really in interrupt context
}

#define MAX_OUTPUT_SIZE (sizeof(struct MLF_resp_packet_header) + sizeof(struct MLF_packet_footer) + 1024)
static uint8_t output_buffer_global[MAX_OUTPUT_SIZE];

static void MLF_resp_data(struct MLF_ctx* ctx, enum MLF_error_codes error, uint8_t* buf, uint16_t len) {
	int ret = 0;
	int delay = MAX_RESPONSE_DELAY;
	uint8_t output_buffer_stack[128];
	uint8_t* output_buffer;

	struct MLF_resp_packet_header header = {
			.magic = MLF_RESP_HEADER_MAGIC,
			.error_code = error,
			.data_size = len
	};
	struct MLF_packet_footer footer = {
			.magic = MLF_FOOTER_MAGIC
	};

	const uint32_t output_buffer_size = sizeof header + sizeof footer + len;
	if(output_buffer_size <= sizeof(output_buffer_stack))
		output_buffer = output_buffer_stack;
	else if(output_buffer_size > MAX_OUTPUT_SIZE) {
		LOG_ERROR("Failed to send response - static buffer is not large enough");
		return ;
	} else
		output_buffer = output_buffer_global;

	memcpy(output_buffer, &header, sizeof header);
	memcpy(output_buffer + sizeof header, buf, len);
	memcpy(output_buffer + sizeof header + len, &footer, sizeof footer);

	while(delay--) {
		ret = ctx->write_func(output_buffer, output_buffer_size);
		if(ret == HAL_OK)
			break;
		else if(ret != HAL_BUSY) {
			LOG_ERROR("Failed to send response - write_func returned %d", ret);
			return;
		}

		// Give it another try
		HAL_Delay(10);
	}

	if(ret == HAL_BUSY)
		LOG_ERROR("Failed to send response - write_func is BUSY");

}

static int MLF_validate_header(struct MLF_ctx* ctx, uint8_t* buf) {
	struct MLF_req_packet_header* pkt = (struct MLF_req_packet_header*) buf;

	if(pkt->magic == MLF_HEADER_MAGIC) {
		// Check fields specific to request header
		if(pkt->cmd >= MLF_CMD_MAX) {
			LOG_ERROR("Header with invalid command was received");
			MLF_resp_error(ctx, MLF_RET_INVALID_CMD);
			return 1;
		}
	} else if(pkt->magic == MLF_RESP_HEADER_MAGIC) {
		// There's n-th to extra validate in response packets
		;
	} else {
		LOG_ERROR("Header with invalid magic was received");
		MLF_resp_error(ctx, MLF_RET_INVALID_HEADER);
		return 1;
	}

	if(pkt->data_size > MLF_MAX_DATA_SIZE){
		LOG_ERROR("Header with too large data size was received");
		MLF_resp_error(ctx, MLF_RET_DATA_TOO_LARGE);
		return 1;
	}

	return 0;
}

static int MLF_validate_footer(struct MLF_ctx* ctx, uint8_t* buf) {
	struct MLF_req_packet_header* pkt = (struct MLF_req_packet_header*) buf;
	struct MLF_packet_footer* footer;

	footer = (struct MLF_packet_footer*)(buf + sizeof(*pkt) + pkt->data_size);

	if(footer->magic != MLF_FOOTER_MAGIC) {
		LOG_ERROR("Footer with invalid magic was received - received [%lx]; expected [%lx]",
					footer->magic, MLF_FOOTER_MAGIC);
		MLF_resp_error(ctx, MLF_RET_INVALID_FOOTER);
		return 1;
	}

	return 0;
}

static void MLF_submit_packet(struct MLF_ctx* ctx, uint8_t* buf, uint32_t len) {
	if(len > PACKET_BUFFER_MAX_SIZE)
		len = PACKET_BUFFER_MAX_SIZE;
	if(ctx->new_data_available) {
		LOG_ERROR("Dropping packet - previous hasn't been processed yet");
		return;
	}

	memcpy(ctx->recv_packet_buf, buf, len);
	ctx->new_data_available = 1;
}

int MLF_is_packet_available(struct MLF_ctx* ctx) {
	return ctx->new_data_available;
}

void MLF_process_packet(struct MLF_ctx* ctx) {
	int ret = MLF_RET_NOT_READY;
	uint8_t response[64];
	uint16_t response_size = 0;
	struct MLF_req_packet_header* hdr;

	if(!ctx->new_data_available)
		return;

	hdr = (struct MLF_req_packet_header*) ctx->recv_packet_buf;

	if(hdr->magic == MLF_RESP_HEADER_MAGIC) {
		// Handle response packet
		if(hdr->cmd != 0)
			LOG_WARN("Received response contains error code - %d", hdr->cmd);
		if(ctx->ops[MLF_CMD_HANDLE_RESPONSE] != NULL) {
			ret = ctx->ops[MLF_CMD_HANDLE_RESPONSE](hdr->data, hdr->data_size, NULL, NULL);
			if(ret != MLF_RET_OK)
				LOG_WARN("Encountered an error while processing response (%d)", ret);
		}

		ctx->new_data_available = 0;
		return;
	}

	if(ctx->ops[hdr->cmd] != NULL)
		ret = ctx->ops[hdr->cmd](hdr->data, hdr->data_size, response, &response_size);
	if(ret != MLF_RET_OK)
		LOG_WARN("Command processing finished with code %d", ret);

	hdr = NULL;
	ctx->new_data_available = 0;
	MLF_resp_data(ctx, ret, response, response_size);
}

void MLF_register_callback(struct MLF_ctx* ctx, enum MLF_commands cmd, MLF_command_handler cb) {
	ctx->ops[cmd] = cb;
}

void MLF_SendCmd(struct MLF_ctx* ctx, enum MLF_commands cmd, uint8_t* data, uint16_t size) {
	int ret = 0;
	int delay = MAX_RESPONSE_DELAY;
	uint8_t output_buffer_stack[128];
	uint8_t* output_buffer;

	struct MLF_req_packet_header header = {
			.magic = MLF_HEADER_MAGIC,
			.cmd = cmd,
			.data_size = size
	};
	struct MLF_packet_footer footer = {
			.magic = MLF_FOOTER_MAGIC
	};

	const uint32_t output_buffer_size = sizeof header + sizeof footer + size;
	if(output_buffer_size <= sizeof(output_buffer_stack))
		output_buffer = output_buffer_stack;
	else if(output_buffer_size > MAX_OUTPUT_SIZE) {
		LOG_ERROR("Failed to send command - static buffer is not large enough");
		return ;
	} else
		output_buffer = output_buffer_global;

	memcpy(output_buffer, &header, sizeof header);
	memcpy(output_buffer + sizeof header, data, size);
	memcpy(output_buffer + sizeof header + size, &footer, sizeof footer);

	while(delay--) {
		ret = ctx->write_func(output_buffer, output_buffer_size);
		if(ret == HAL_OK)
			break;
		else if(ret != HAL_BUSY) {
			LOG_ERROR("Failed to send command - write_func returned %d", ret);
			return;
		}

		// Give it another try
		HAL_Delay(10);
	}

	if(ret == HAL_BUSY)
		LOG_ERROR("Failed to send command - write_func is BUSY");
}
