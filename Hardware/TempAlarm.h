#ifndef __TEMPALARM_H
#define __TEMPALARM_H

#include "stm32f1xx_hal.h"

typedef enum
{
	TEMP_ALARM_STATE_NORMAL = 0,  //正常
	TEMP_ALARM_STATE_HIGH       //高温报警
}TempAlarm_State_t;

void TempAlarm_Init(void);
void TempAlarm_Update(float temperature);
TempAlarm_State_t TempAlarm_GetState(void);

#endif
