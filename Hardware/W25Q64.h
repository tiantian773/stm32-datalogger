#ifndef __W25Q64_H
#define __W25Q64_H

#include "stm32f1xx_hal.h"

void W25Q64_Init(void);
HAL_StatusTypeDef W25Q64_ReadID(uint8_t *MID,uint16_t *DID);
HAL_StatusTypeDef W25Q64_WriteEnable(void);
HAL_StatusTypeDef W25Q64_WaitRead(uint32_t Timeout);
HAL_StatusTypeDef W25Q64_PageProgram(uint32_t Address,uint8_t *DataArray,uint16_t Count);
HAL_StatusTypeDef W25Q64_SectorErase(uint32_t Address);
HAL_StatusTypeDef W25Q64_ReadData(uint32_t Address,uint8_t *DataArray,uint32_t Count);

#endif

