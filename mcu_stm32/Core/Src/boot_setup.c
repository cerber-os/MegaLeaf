// SPDX-License-Identifier: GPL-2.0-only
/*
 * boot_setup.c - 
 *  (C) 2022 Pawel Wieczorek
 */

#include <stm32f4xx_hal.h>


/**************************
 * Reset reason stuff
 **************************/
static uint32_t reset_reason;

/*
 * List describing each of possible reset reason flags set
 *  by RCC. Order here is important: some of the flags are
 *  only valid after checking that the previous ones are not
 *  (BORRSTF for instance)
 */
struct reset_reason_desc {
	const int id;
	const char* str;
};
#define HAL_RCC_TO_ID(X)	((X) & RCC_FLAG_MASK)
static const struct reset_reason_desc RESET_REASONS[] = {
		{.id = HAL_RCC_TO_ID(RCC_FLAG_LPWRRST), "Low-power management reset"},
		{.id = HAL_RCC_TO_ID(RCC_FLAG_WWDGRST), "Window watchdog reset"},
		{.id = HAL_RCC_TO_ID(RCC_FLAG_IWDGRST), "Independent watchdog reset"},
		{.id = HAL_RCC_TO_ID(RCC_FLAG_SFTRST), "Software reset"},
		{.id = HAL_RCC_TO_ID(RCC_FLAG_PORRST), "POR/PDR reset"},
		{.id = HAL_RCC_TO_ID(RCC_FLAG_BORRST), "BOR reset"},
		{.id = HAL_RCC_TO_ID(RCC_FLAG_PINRST), "NRST external reset"},
};

void boot_save_reset_reason(void) {
	reset_reason = RCC->CSR;
	__HAL_RCC_CLEAR_RESET_FLAGS();
}

int boot_get_reset_reason_id(void) {
	return reset_reason;
}

const char* boot_get_reset_reason_str(void) {
	for(int i = 0; i < sizeof(RESET_REASONS) / sizeof(RESET_REASONS[0]); i++) {
		if(reset_reason & (1UL << RESET_REASONS[i].id))
			return RESET_REASONS[i].str;
	}
	return "Unknown reset cause";
}

