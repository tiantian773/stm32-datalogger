#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line,uint8_t Column,char Char);
void OLED_ShowString(uint8_t Line,uint8_t Column,char *String);
void OLED_ShowNum(uint8_t Line,uint8_t Column,uint32_t Number,uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column,int32_t Number,uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column,int32_t Number,uint8_t Length);
void OLED_ShowBinNum(uint8_t Line,uint8_t Column,uint32_t Number,uint8_t Length);
void OLED_ShowFloat(uint8_t Line, uint8_t Column, float Number,uint8_t Width, uint8_t Precision);

void OLED_ShowChineseChar(uint8_t Line,uint8_t Column,const char *Text);
void OLED_ShowChineseString(uint8_t Line,uint8_t Column,const char *Text);
void OLED_ShowText(uint8_t Line,uint8_t Column,const char *Text);
void OLED_Init(void);
void OLED_BeginUpdate(void);


#endif
