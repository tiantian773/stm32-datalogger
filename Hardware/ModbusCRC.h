#ifndef __MODBUS_CRC_H
#define __MODBUS_CRC_H

#include "stm32f1xx_hal.h"

uint16_t Modbus_CRC16(const uint8_t *data, uint16_t length);
uint8_t Modbus_CheckCRC(const uint8_t *frame, uint16_t length);

#endif
