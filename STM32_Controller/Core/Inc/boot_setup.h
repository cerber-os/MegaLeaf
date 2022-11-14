// SPDX-License-Identifier: GPL-2.0-only
/*
 * boot_setup.h - 
 *  (C) 2021 Pawel Wieczorek
 */
#ifndef INC_BOOT_SETUP_H_
#define INC_BOOT_SETUP_H_


void boot_save_reset_reason(void);
int boot_get_reset_reason_id(void);
const char* boot_get_reset_reason_str(void);


#endif /* INC_BOOT_SETUP_H_ */
