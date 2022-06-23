// SPDX-License-Identifier: GPL-2.0-only
/*
 * mlf_protocol.c - 
 *  (C) 2021 Pawel Wieczorek
 */

#include "mlf_protocol.h"

#include "stm32l5xx_hal.h"
#include "usbd_cdc_if.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern CRC_HandleTypeDef hcrc;

static void MLF_resp_error(enum MLF_error_codes error);
static int MLF_validate_header(uint8_t* buf);
static int MLF_validate_footer(uint8_t* buf);
static void MLF_submit_packet(uint8_t* buf, uint32_t len);

/**********************
 * LOGGING SUPPORT
 **********************/
#define LOG_INFO(MSG, ...)		printf("[INFO] |%s| " MSG "\n", __func__, ##__VA_ARGS__);
#define LOG_WARN(MSG, ...)		printf("[WARN] |%s| " MSG "\n", __func__, ##__VA_ARGS__);
#define LOG_ERROR(MSG, ...)		printf("[ERROR]|%s| " MSG "\n", __func__, ##__VA_ARGS__);

void hexdump(uint8_t* data, size_t len) {
	for(int i = 0; i < len; i++) {
		unsigned int x = data[i];
		printf("%02x", x);
	}
	printf("\n");
}

/**********************
 * PACKET BUFFER SUPPORT
 *  - for processing data received via USB CDC, we need
 *    simple buffer to which data can be appended and which
 *    will drop partial transfers
 **********************/
#define PACKET_BUFFER_MAX_DELAY		1000
#define PACKET_BUFFER_MAX_SIZE		(MLF_MAX_DATA_SIZE + \
										sizeof(struct MLF_req_packet_header) + \
										sizeof(struct MLF_packet_footer))

struct packet_buffer {
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

struct packet_buffer* packet_buffer_init(void) {
	struct packet_buffer* pkt;

	pkt = malloc(sizeof *pkt);
	if(pkt == NULL) {
		LOG_ERROR("Failed to allocate memory for new packet_buffer");
		return NULL;
	}

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
		MLF_resp_error(MLF_RET_DATA_TOO_LARGE);
		return -1;
	}

	// Append new data to our packed buffer
	memcpy(pkt->buffer + pkt->size, buf, len);
	pkt->size += len;

	// Validate header and footer of received data
	if(!pkt->header_validated && pkt->size >= sizeof(struct MLF_req_packet_header)) {
		ret = MLF_validate_header(pkt->buffer);
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
		ret = MLF_validate_footer(pkt->buffer);
		if(ret) {
			// MLF_validate_footer already reports an error to host
			packet_buffer_clear(pkt);
			return -1;
		}

		// Full packet has been received
		MLF_submit_packet(pkt->buffer, pkt->size);
		packet_buffer_clear(pkt);
		return 0;
	}

	pkt->time_last_packet = HAL_GetTick();
	return 0;
}


/**********************
 * MegaLeaf (MLF) Packet Support
 **********************/
#define MAX_RESPONSE_DELAY		30
static MLF_command_handler MLF_operations[MLF_CMD_MAX] = {0};

uint8_t recv_packet_len;
uint8_t* recv_packet_buf;
volatile uint8_t new_data_available = 0;

int MLF_init(void) {
	recv_packet_buf = malloc(PACKET_BUFFER_MAX_SIZE);
	if(recv_packet_buf == NULL)
		return -1;

	memset(recv_packet_buf, 0, PACKET_BUFFER_MAX_SIZE);
	return 0;
}

static void MLF_resp_error(enum MLF_error_codes error) {
	int ret = 0;
	struct MLF_resp_packet_header header = {
			.magic = MLF_HEADER_MAGIC,
			.error_code = error,
			.data_size = 0
	};
	struct MLF_packet_footer footer = {
			.magic = MLF_FOOTER_MAGIC
	};

	int crcLen = (sizeof header & 0xfffffffc) / 4;
	footer.crc = ~ HAL_CRC_Calculate(&hcrc, (uint32_t*) &header, crcLen);

	uint8_t output_buffer[sizeof header + sizeof footer];
	memcpy(output_buffer, &header, sizeof(header));
	memcpy(output_buffer + sizeof header, &footer, sizeof(footer));

	ret = CDC_Transmit_FS(output_buffer, sizeof output_buffer);
	if(ret)
		LOG_ERROR("Failed to send error reponse - CDC_Transmit_FS returned %d", ret);
		// We're in interrupt context here, so there's literally nth we could do about
		//  this error. Normally, if it's USB_BUSY error, we would sleep a while, but
		//  here, it's better to just give up
}

#define MAX_OUTPUT_SIZE (sizeof(struct MLF_resp_packet_header) + sizeof(struct MLF_packet_footer) + 1024)

uint8_t output_buffer[MAX_OUTPUT_SIZE];
static void MLF_resp_data(enum MLF_error_codes error, uint8_t* buf, uint16_t len) {
	int ret = 0;
	int crcLen;
	int delay = MAX_RESPONSE_DELAY;
	struct MLF_resp_packet_header header = {
			.magic = MLF_HEADER_MAGIC,
			.error_code = error,
			.data_size = len
	};
	struct MLF_packet_footer footer = {
			.magic = MLF_FOOTER_MAGIC
	};

	const uint32_t output_buffer_size = sizeof header + sizeof footer + len;

	if(output_buffer_size > MAX_OUTPUT_SIZE) {
		LOG_ERROR("Failed to send response - static buffer is not large enough");
		return ;
	}

	memcpy(output_buffer, &header, sizeof header);
	memcpy(output_buffer + sizeof header, buf, len);

	crcLen = (sizeof header + len) & 0xfffffffc;
	crcLen /= 4;
	footer.crc = ~ HAL_CRC_Calculate(&hcrc, (uint32_t*) output_buffer, crcLen);
	memcpy(output_buffer + sizeof header + len, &footer, sizeof footer);

	while(delay--) {
		ret = CDC_Transmit_FS(output_buffer, output_buffer_size);
		if(ret == USBD_OK)
			break;
		else if(ret != USBD_BUSY) {
			LOG_ERROR("Failed to send response - CDC_Transmit_FS returned %d", ret);
			return;
		}

		// Give it another try
		HAL_Delay(10);
	}

	if(ret == USBD_BUSY)
		LOG_ERROR("Failed to send response - CSC_Transmit_FS is BUSY");

	// LOG_INFO("Sent response packet do host");
}

static int MLF_validate_header(uint8_t* buf) {
	struct MLF_req_packet_header* pkt = (struct MLF_req_packet_header*) buf;

	if(pkt->magic != MLF_HEADER_MAGIC) {
		LOG_ERROR("Header with invalid magic was received");
		MLF_resp_error(MLF_RET_INVALID_HEADER);
		return 1;
	}
	if(pkt->cmd >= MLF_CMD_MAX) {
		LOG_ERROR("Header with invalid command was received");
		MLF_resp_error(MLF_RET_INVALID_CMD);
		return 1;
	}
	if(pkt->data_size > MLF_MAX_DATA_SIZE){
		LOG_ERROR("Header with too large data size was received");
		MLF_resp_error(MLF_RET_DATA_TOO_LARGE);
		return 1;
	}

	return 0;
}

static int MLF_validate_footer(uint8_t* buf) {
	uint32_t crc, crcLen;
	struct MLF_req_packet_header* pkt = (struct MLF_req_packet_header*) buf;
	struct MLF_packet_footer* footer;

	footer = (struct MLF_packet_footer*)(buf + sizeof(*pkt) + pkt->data_size);

	if(footer->magic != MLF_FOOTER_MAGIC) {
		LOG_ERROR("Footer with invalid magic was received - received [%lx]; expected [%x]",
					footer->magic, MLF_FOOTER_MAGIC);
		MLF_resp_error(MLF_RET_INVALID_FOOTER);
		return 1;
	}

	// Round to the closest full word boundary and convert to the number of 32-bit words
	crcLen = (sizeof(*pkt) + pkt->data_size) & 0xfffffffc;
	crcLen /= 4;

	crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)buf, crcLen);
	crc = ~crc;
	if(crc != footer->crc) {
		LOG_ERROR("CRC mismatch - local [%lx]; received [%lx]", crc, footer->crc);
		long unsigned int testData = 0;
		printf("CRC test - 0x00000000: %lx\n", HAL_CRC_Calculate(&hcrc, &testData, sizeof(testData) / 4));
		// MLF_resp_error(MLF_RET_CRC_FAIL);
		// TODO: IGNORE temporarilt
		// return 1;
	}

	return 0;
}

static void MLF_submit_packet(uint8_t* buf, uint32_t len) {
	// LOG_INFO("New packet submitted: len=%lu", len);

	if(!recv_packet_buf)
		return;
	if(len > PACKET_BUFFER_MAX_SIZE)
		len = PACKET_BUFFER_MAX_SIZE;
	if(new_data_available) {
		LOG_ERROR("Dropping packet - previous hasn't been processed yet");
		return;
	}

	memcpy(recv_packet_buf, buf, len);
	new_data_available = 1;
}

int MLF_is_packet_available(void) {
	return new_data_available;
}

void MLF_process_packet(void) {
	int ret = MLF_RET_NOT_READY;
	uint8_t response[64];
	uint16_t response_size = 0;
	struct MLF_req_packet_header* hdr;

	if(!new_data_available)
		return;

	hdr = (struct MLF_req_packet_header*) recv_packet_buf;

	//LOG_INFO("Processing new packet for command %d", hdr->cmd);
	if(MLF_operations[hdr->cmd] != NULL)
		ret = MLF_operations[hdr->cmd](hdr->data, hdr->data_size, response, &response_size);
	if(ret != MLF_RET_OK)
		LOG_WARN("Command processing finished with code %d", ret);

	hdr = NULL;
	new_data_available = 0;
	MLF_resp_data(ret, response, response_size);
}

void MLF_register_callback(enum MLF_commands cmd, MLF_command_handler cb) {
	MLF_operations[cmd] = cb;
}
