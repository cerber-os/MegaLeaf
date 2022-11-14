// SPDX-License-Identifier: GPL-2.0-only
/*
 * mlf_protocol.h - definition of protocol to communicate with PC and ESP32
 *  (C) 2021 Pawel Wieczorek
 */
#ifndef INC_MLF_PROTOCOL_H_
#define INC_MLF_PROTOCOL_H_

#include <stdint.h>

#ifdef _MSC_VER
	#define PACKED
	__pragma(pack(push, 1))
#else
	#define PACKED	__attribute__((packed))
#endif

/**********************
 * MLF PROTOCOL DEFINITION
 **********************/
#define MLF_HEADER_MAGIC			0x004D4C46UL
#define MLF_RESP_HEADER_MAGIC		0x00524C4DUL
#define MLF_FOOTER_MAGIC			0x7364656CUL
#define MLF_MAX_DATA_SIZE			2048

enum MLF_commands {
	MLF_CMD_TURN_OFF		= 0,
	MLF_CMD_TURN_ON,
	MLF_CMD_GET_INFO,

	MLF_CMD_SET_BRIGHTNESS,
	MLF_CMD_SET_COLOR,
	MLF_CMD_SET_EFFECT,

	MLF_CMD_GET_BRIGHTNESS,
	MLF_CMD_GET_EFFECT,
	MLF_CMD_GET_ON_STATE,

	MLF_CMD_MAX,

	// Nasty, but response is a special type of command with
	//  different magic, format, etc. That's why it's put after
	//  MLF_CMD_MAX enumerator
	MLF_CMD_HANDLE_RESPONSE
};

enum MLF_error_codes {
	MLF_RET_OK				= 0,
	MLF_RET_PING			= 1,

	MLF_RET_INVALID_CMD		= 128,
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
} PACKED;

struct MLF_resp_packet_header {
	uint32_t 	magic;
	uint8_t		error_code;
	uint16_t 	data_size;
	uint8_t		data[0];
} PACKED;

struct MLF_packet_footer {
	uint32_t magic;
} PACKED;


/*
 * MLF_CMD_GET_INFO
 */
struct MLF_resp_cmd_get_info {
	uint8_t fw_version;
	uint16_t leds_count_top;
	uint16_t leds_count_bottom;
} PACKED;

/*
 * MLF_CMD_SET_BRIGHTNESS
 */
#define MLF_REQ_CMD_SET_BRIGHTNESS_LEN		(sizeof struct MLF_req_cmd_set_brightness)

struct MLF_req_cmd_set_brightness {
	uint8_t brightness;
	uint8_t strip;
} PACKED;

/*
 * MLF_CMD_SET_COLOR
 */
#define MLF_REQ_CMD_SET_COLOR_LEN			(sizeof struct MLF_req_cmd_set_color)

struct MLF_req_cmd_set_color {
	uint8_t strip;
	uint8_t colors[0];
} PACKED;

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
} PACKED;

/*
 * MLF_CMD_GET_BRIGHTNESS
 */
#define MLF_RESP_CMD_GET_BRIGHTNESS_LEN	(sizeof struct MLF_resp_cmd_get_brightness)

struct MLF_resp_cmd_get_brightness {
	uint8_t brightness;
};

/*
 * MLF_CMD_GET_EFFECT
 */
#define MLF_RESP_CMD_GET_EFFECT_LEN		(sizeof struct MLF_resp_cmd_get_effect)

struct MLF_resp_cmd_get_effect {
	uint8_t effect;
	uint8_t speed;
	uint32_t color;
};

/*
 * MLF_CMD_GET_ON_STATE
 */
#define MLF_RESP_CMD_GET_ON_STATE_LEN	(sizeof struct MLF_resp_cmd_get_on_state)

struct MLF_resp_cmd_get_on_state {
	uint8_t is_on;
};


/**********************
 * PACKET BUFFER FOR INCOMMING TRANSMISSION
 **********************/
struct MLF_ctx;
struct packet_buffer;

struct packet_buffer* packet_buffer_init(struct MLF_ctx* ctx);
void packet_buffer_deinit(struct packet_buffer*);
int packet_buffer_append(struct packet_buffer*, uint8_t*, uint32_t);


/**********************
 * PACKETS PROCESSING
 **********************/
typedef int (*MLF_command_handler)(uint8_t*, uint16_t, uint8_t*, uint16_t*);
typedef int (*MLF_write_func)(uint8_t*, uint16_t);

enum MLF_OPTS {
	MLF_OPTS_NONE				= 0,
	MLF_OPTS_SEND_STATE_CHANGE	= 1 << 0,
};

struct MLF_ctx {
	MLF_write_func write_func;
	MLF_command_handler ops[MLF_CMD_HANDLE_RESPONSE + 1];
	uint16_t recv_packet_len;
	uint8_t* recv_packet_buf;
	uint8_t new_data_available;
	uint8_t opts;
};

struct MLF_reroute {
	struct MLF_ctx* from;
	struct MLF_ctx* to;
	uint32_t routed_cmds;
};

int MLF_init(struct MLF_ctx*, MLF_write_func);
int MLF_is_packet_available(struct MLF_ctx* ctx);
void MLF_process_packet(struct MLF_ctx* ctx);
void MLF_register_callback(struct MLF_ctx* ctx, enum MLF_commands cmd, MLF_command_handler cb);
void MLF_register_reroute(struct MLF_ctx* from, struct MLF_ctx* to, enum MLF_commands cmd);
void MLF_SendCmd(struct MLF_ctx* ctx, enum MLF_commands cmd, uint8_t* data, uint16_t size);

#ifdef _MSC_VER
	__pragma(pack(pop))
#endif

#endif /* INC_MLF_PROTOCOL_H_ */
