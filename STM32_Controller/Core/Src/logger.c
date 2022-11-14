// SPDX-License-Identifier: GPL-2.0-only
/*
 * logger.c - 
 *  (C) 2021 Pawel Wieczorek
 */

#include "logger.h"

#include <stm32f4xx_hal.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Private macros */
#define ANSI_TERM_RESET		"\x1b[0m"
#define ANSI_TERM_GREEN		"\x1b[0;32m"
#define ANSI_TERM_YELLOW	"\x1b[0;32m"
#define ANSI_TERM_RED		"\x1b[0;31m"
#define ANSI_TERM_BOLD_RED	"\x1b[1;31m"
#define ANSI_TERM_CYAN		"\x1b[0;36m"
#define ANSI_TERM_BOLD_WHITE "\x1b[1;39m"


#define LOG_FMT_TIME		ANSI_TERM_GREEN "[%08lu] "
#define LOG_FMT_HEADER(HDR)	ANSI_TERM_YELLOW HDR ": "

enum log_levels {
	LOGLEVEL_EMERG = 0,
	LOGLEVEL_ERR,
	LOGLEVEL_WARNING,
	LOGLEVEL_INFO,
	LOGLEVEL_DEBUG,

	LOGLEVEL_CONT,

	LOGLEVEL_DEFAULT = LOGLEVEL_INFO,
};

static int header_to_log(const char* fmt, enum log_levels* level) {
	if(fmt[0] != LOG_SOH[0]) {
		*level = LOGLEVEL_DEFAULT;
		return 0;
	}

	if(fmt[1] >= LOG_EMERG[1] || fmt[1] <= LOG_DEBUG[1])
		*level = fmt[1] - LOG_EMERG[1];
	else if(fmt[1] == LOG_CONT[1])
		*level = LOGLEVEL_CONT;
	else
		*level = LOGLEVEL_DEFAULT;
	return 1;
}

static const char* loglvl_to_color(enum log_levels lvl) {
	if(lvl == LOGLEVEL_EMERG)
		return ANSI_TERM_BOLD_RED;
	else if(lvl == LOGLEVEL_ERR)
		return ANSI_TERM_RED;
	else if(lvl == LOGLEVEL_WARNING)
		return ANSI_TERM_BOLD_WHITE;
	else if(lvl == LOGLEVEL_DEBUG)
		return ANSI_TERM_CYAN;
	return ANSI_TERM_RESET;
}

void printk(const char* fmt, ...) {
	uint32_t timestamp;
	enum log_levels level;

	if(header_to_log(fmt, &level))
		fmt += 2;

	if(level != LOGLEVEL_CONT) {
		timestamp = HAL_GetTick();
		printf("\n" LOG_FMT_TIME, timestamp);
	}

	printf("%s", loglvl_to_color(level));

	va_list args;
	va_start (args, fmt);
	vprintf(fmt, args);
	va_end(args);

	printf("%s", ANSI_TERM_RESET);
	fflush(stdout);
}

