// SPDX-License-Identifier: GPL-2.0-only
/*
 * mlf_protocol.h - 
 *  (C) 2021 Pawel Wieczorek
 */
#ifndef INC_MLF_PROTOCOL_H_
#define INC_MLF_PROTOCOL_H_

#include <stdint.h>


/**********************
 * MLF PROTOCOL DEFINITION
 **********************/
#define MLF_HEADER_MAGIC			0x004D4C46UL
#define MLF_FOOTER_MAGIC			0x7364656CUL
#define MLF_MAX_DATA_SIZE			2048

enum MLF_commands {
	MLF_CMD_TURN_OFF		= 0,
	MLF_CMD_GET_INFO,

	MLF_CMD_FEATURE_PING,
	MLF_CMD_FEATURE_PING_CONFIG,

	MLF_CMD_SET_BRIGHTNESS,
	MLF_CMD_SET_COLOR,
	MLF_CMD_SET_EFFECT,

	MLF_CMD_MAX
};

enum MLF_error_codes {
	MLF_RET_OK				= 0,
	MLF_RET_PING			= 1,

	MLF_RET_CRC_FAIL		= 128,
	MLF_RET_INVALID_CMD,
	MLF_RET_INVALID_HEADER,
	MLF_RET_INVALID_DATA,
	MLF_RET_INVALID_FOOTER,
	MLF_RET_NOT_READY,
	MLF_RET_TIMEOUT,
	MLF_RET_DATA_TOO_LARGE
};

struct MLF_req_packet_header {
	uint32_t	magic;
	uint8_t		cmd;
	uint16_t	data_size;
	uint8_t		data[0];
} __attribute__((packed));

struct MLF_resp_packet_header {
	uint32_t 	magic;
	uint8_t		error_code;
	uint16_t 	data_size;
	uint8_t		data[0];
} __attribute__((packed));

struct MLF_packet_footer {
	uint32_t crc;
	uint32_t magic;
} __attribute__((packed));


/*
 * MLF_CMD_GET_INFO
 */
struct MLF_resp_cmd_get_info {
	uint8_t fw_version;
	uint16_t leds_count_top;
	uint16_t leds_count_bottom;
} __attribute__((packed));

/*
 * MLF_CMD_SET_BRIGHTNESS
 */
#define MLF_REQ_CMD_SET_BRIGHTNESS_LEN		(sizeof struct MLF_req_cmd_set_brightness)

struct MLF_req_cmd_set_brightness {
	uint8_t brightness;
	uint8_t strip;
} __attribute__((packed));

/*
 * MLF_CMD_SET_COLOR
 */
#define MLF_REQ_CMD_SET_COLOR_LEN			(sizeof struct MLF_req_cmd_set_color)

struct MLF_req_cmd_set_color {
	uint8_t strip;
	uint8_t colors[0];
} __attribute__((packed));

/*
 * MLF_CMD_SET_EFFECT
 */
#define MLF_REQ_CMD_SET_EFFECT_LEN			(sizeof struct MLF_req_cmd_set_effect)

enum MLF_STRIP_ID {
	STRIP_TOP 		= 0b01,
	STRIP_BOTTOM 	= 0b10,
};

struct MLF_req_cmd_set_effect {
	uint8_t effect;
	uint8_t speed;
	uint8_t strip;
	uint32_t color;
} __attribute__((packed));


/**********************
 * PACKET BUFFER FOR USB CDC DRIVER
 **********************/
struct packet_buffer;

struct packet_buffer* packet_buffer_init(void);
void packet_buffer_deinit(struct packet_buffer*);
int packet_buffer_append(struct packet_buffer*, uint8_t*, uint32_t);


/**********************
 * PACKETS PROCESSING
 **********************/
typedef int (*MLF_command_handler)(uint8_t*, uint16_t, uint8_t*, uint16_t*);

int MLF_init(void);
int MLF_is_packet_available(void);
void MLF_process_packet(void);
void MLF_register_callback(enum MLF_commands cmd, MLF_command_handler cb);

#endif /* INC_MLF_PROTOCOL_H_ */
