#ifndef __ANALOGINPUT_H
#define __ANALOGINPUT_H

#include "stm32f1xx_hal.h"

HAL_StatusTypeDef AnalogInput_Start(void);
HAL_StatusTypeDef AnalogInput_GetRaw(uint16_t *Data1,uint16_t *Data2);
HAL_StatusTypeDef AnalogInput_GetVoltage(float *Voltage1,float *Voltage2);
HAL_StatusTypeDef AnalogInput_GetAverageRaw(uint16_t *Average1,uint16_t *Average2);

#endif
