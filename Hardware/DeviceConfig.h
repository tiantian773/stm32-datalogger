#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>

typedef struct
{
	int16_t TemperatureLimitX100; //温度上限，单位0.01℃
	uint16_t HysteresisX100;      //回差，单位0.01℃
	uint16_t LogIntervalSeconds;  //记录间隔，单位秒
	uint16_t SlaveAddress;        // 从站地址
}DeviceConfig_t;

void DeviceConfig_Init(void);
const DeviceConfig_t *DeviceConfig_Get(void);
uint32_t DeviceConfig_GetRevision(void);
uint8_t DeviceConfig_Validate(const DeviceConfig_t *config);
uint8_t DeviceConfig_Apply(const DeviceConfig_t *config);

#endif
