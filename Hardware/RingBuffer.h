#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

#include "stm32f1xx_hal.h"

#define RING_BUFFER_FRAME_END 0x0100U

void RingBuffer_Init(void);
uint8_t RingBuffer_Push(uint16_t Data);
uint8_t RingBuffer_Pop(uint16_t *data);

#endif
