// SPDX-License-Identifier: GPL-2.0-only
/*
 * panic.c - 
 *  (C) 2022 Pawel Wieczorek
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "panic.h"

#include "stm32f4xx_hal.h"

static void generate_cpu_state(const struct raw_regs* raw, struct pt_regs* regs) {
	memset(regs, 0, sizeof(*regs));
	return;

	// TODO:
	// Start by checking whether cause was triggered by interrupt
	/*if(raw->lr & EXC_RET_MASK) {
		// Interrpt has been caused by userspace
	} else {
		// Interrupt has been caused by another interrupt
	}*/


}

static void print_pt_regs(struct pt_regs* regs) {
	puts("Registers:");
	for(int i = 0; i < 12; i += 2) {
		printf("\t R%d:%s 0x%08lx", i, i < 10 ? " " : "", regs->r[i]);
		printf("  R%d:%s 0x%08lx\n",i + 1, i + 1 < 10 ? " " : "", regs->r[i + 1]);
	}
	printf("\t SP:  0x%08lx\n", regs->sp);
	printf("\t LR:  0x%08lx\n", regs->lr);
	printf("\t PC:  0x%08lx\n", regs->pc);
	printf("\t MSP: 0x%08lx  PSP: 0x%08lx", regs->msp, regs->psp);
}

static void panic_header(const char* msg) {
	int padding = 0;
	int msg_len = strlen(msg);

	if(msg_len < 34)
		padding = (34 - msg_len) / 2;


	puts("\n");
	puts("**************************************");
	puts("*            SYSTEM PANIC            *");
	printf("* %*s *\n", padding, msg);
	puts("**************************************");
}

__attribute__((noreturn)) static void system_reset(void) {
	puts("--[ Rebooting ]--");
	NVIC_SystemReset();

	while(1) ;
}

/***************************
 * Exceptions handlers
 ***************************/
static int NMI_IRQHandler(struct pt_regs* regs) {
	// Redirect Clock Source exception to STM32 HAL library
	if(__HAL_RCC_GET_IT(RCC_IT_CSS)) {
		HAL_RCC_CSSCallback();
		__HAL_RCC_CLEAR_IT(RCC_IT_CSS);

		// If MCU has to be rebooted, it will be triggered
		//  from HAL CSS callback function
		return EXC_HANDLER_IGNORE;
	}

	return EXC_HANDLER_PANIC;
}

static struct exception exceptions[] = {
		[HardFaultExc_ID] = {"HardFault", NULL},
		[NMIExc_ID] = {"Non-maskable interrupt", NMI_IRQHandler},
		[MemManageExc_ID] = {"Memory Manage", NULL},
		[BusFaultExc_ID] = {"Bus Fault", NULL},
		[UsageFaultExc_ID] = {"Usage Fault", NULL},
};


/***************************
 * Exported functions
 ***************************/
/**
 * Called by assembly code to handle interrupt
 * @param raw_regs pointer to dumped context
 * @param exc_id identifier of exception to be handled
 */
void handle_exception(const struct raw_regs* raw_regs, uint32_t exc_id) {
	int should_reboot = 1;
	struct exception* exc = NULL;
	struct pt_regs regs;

	if(exc_id >= sizeof(exceptions)) {
		panic_header("Unknown exception");
		printf(" Ooops, Unknown exception ID(%ld)! This shouldn't have happened\n", exc_id);
		puts(" Quickly, reboot the MCU before someone realises that");
		system_reset();
	}

	exc = &exceptions[exc_id];
	panic_header(exc->exc_name);

	generate_cpu_state(raw_regs, &regs);

	if(exc->exc_handler)
		should_reboot = exc->exc_handler(&regs);

	print_pt_regs(&regs);

	if(should_reboot)
		system_reset();
	else {
		puts("Handler requested to NOT reboot the MCU");
		puts("Dazed and confused, but trying to continue...");
	}
}

/**
 * Print error message to DBG UART port and reboot MCU
 * @param error_msg error message
 */
__attribute__((noreturn)) void panic(const char* error_msg) {
	uint32_t lr;
	__asm volatile("mov %0, lr" : "=r" (lr) : );

	panic_header("Fatal system error");
	printf(" BUG: %s\n", error_msg);
	printf(" Triggered by: 0x%08lx\n", lr - 4);
	system_reset();
}
