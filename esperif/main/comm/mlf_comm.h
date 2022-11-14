/**
 * @file mlf_comm.h
 * @author Pawel Wieczorek
 * @brief Compatiblity layer for MLF controller UART protocol
 * @date 2022-07-31
 * 
 */

#ifndef INC_MLF_COMM_H
#define INC_MLF_COMM_H

#include "mlf_protocol.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void MLF_Comm_init(void);
int MLF_Comm_sendCmd(enum MLF_commands cmd, void* data, uint16_t size, void* resp, size_t* resp_size);
void MLF_Comm_setCallback(enum MLF_commands cmd, MLF_command_handler cb);

int MLF_Comm_SetBrightness(uint16_t brightness);
int MLF_Comm_SetEffect(uint8_t mode, uint8_t speed, uint8_t strip, uint32_t color);

int MLF_Comm_TurnOff(void);
int MLF_Comm_TurnOn(void);

int MLF_Comm_GetBrightness(uint16_t* brightness);
int MLF_Comm_GetEffect(uint16_t* effect, uint16_t* speed, uint32_t* color);
int MLF_Comm_GetOnState(uint8_t* on_state);

#ifdef __cplusplus
}
#endif

#endif
