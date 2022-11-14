// SPDX-License-Identifier: GPL-2.0-only
/*
 * panic.h - 
 *  (C) 2021 Pawel Wieczorek
 */
#ifndef INC_PANIC_H_
#define INC_PANIC_H_

#include <stdint.h>

/**
 * raw_regs - context dumped performed by assembly
 */
struct raw_regs {
	uint32_t fp;
	uint32_t sp;
	uint32_t r[13];
	uint32_t lr;
	uint32_t msp, psp;
} __attribute__((packed));

/**
 * pt_regs - structure representing the state of MCU
 *   in the moment the exception has happened
 *  Obtained by processing raw_regs structure
 */
struct pt_regs {
	uint8_t is_interrupt;
	uint8_t is_supervisor;
	uint32_t r[13];
	uint32_t lr;
	uint32_t sp;
	uint32_t pc;
	uint32_t msp, psp;
};

/**
 * exception: structure describing exception ID
 *  exc_name - exception name
 *  exc_handler - optional function performing additional
 *    exception handling; return !0 to reboot MCU
 */
struct exception {
	const char* exc_name;
	const int (*exc_handler)(struct pt_regs*);
};

#define EXC_HANDLER_IGNORE		0
#define EXC_HANDLER_PANIC		1

/**
 * Exception IDs set by panic.S
 *  Use #define as assembly doesn't support enums
 */
#define HardFaultExc_ID		0
#define NMIExc_ID			1
#define MemManageExc_ID		2
#define BusFaultExc_ID		3
#define UsageFaultExc_ID	4

/***************************
 * Exported functions
 ***************************/
/**
 * Panic the MCU - print error to serial console and reboot
 * @param error_msg
 */
void panic(const char* error_msg) __attribute__((noreturn));

/**
 * Used by assembly code to handle exception in C
 * @param raw_regs pointer to dumped context
 * @param exc_id ID of exception
 */
void handle_exception(const struct raw_regs* raw_regs, uint32_t exc_id);

#endif /* INC_PANIC_H_ */
