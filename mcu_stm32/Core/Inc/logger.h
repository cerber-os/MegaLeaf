// SPDX-License-Identifier: GPL-2.0-only
/*
 * logger.h - 
 *  (C) 2022 Pawel Wieczorek
 */
#ifndef INC_LOGGER_H_
#define INC_LOGGER_H_

/* Log prefixes based on Linux kernel ones */
#define LOG_SOH			"\x001"
#define LOG_EMERG		LOG_SOH "0"			/* system is unusable */
#define LOG_ERR			LOG_SOH "1"			/* error conditions */
#define LOG_WARNING		LOG_SOH "2"			/* warning conditions */
#define LOG_INFO		LOG_SOH "3"			/* informational */
#define LOG_DEBUG		LOG_SOH "4"			/* debug-level messages */

#define LOG_CONT		LOG_SOH "c"			/* continue previous line */


/**
 * printk - print a meesage to debug port
 * @param fmt format string
 */
void printk(const char* fmt, ...);

/**
 * printd - print a debug message
 *
 * On non-debug build this function is empty, so it can
 *  be used in stress locations
 */
#ifdef CORE_DEBUG
#define printd(FMT, ...)	printk(LOG_DEBUG FMT, ##__VA_ARGS__)
#else
#define printd(FMT, ...)	do {} while(0)
#endif


#endif /* INC_LOGGER_H_ */
