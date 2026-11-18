#include "DeviceConfig.h"
#include <stddef.h>

static DeviceConfig_t currentConfig;//设备配置，保存当前使用的参数。static 将它限制在本文件内，其他模块通过接口访问。
static uint32_t configRevision=0U;//配置修订号。每次成功修改就加一，主循环据此判断是否需要更新报警和记录周期。

void DeviceConfig_Init(void)
{
	currentConfig.TemperatureLimitX100=3100; //温度上线
	currentConfig.HysteresisX100=100U;       //回差
	currentConfig.LogIntervalSeconds=1U;     //记录间隔，单位s
	currentConfig.SlaveAddress=1U;           //从机地址
	
	configRevision=1U;                       //配置修订号
}



const DeviceConfig_t *DeviceConfig_Get(void)//该地址指向的参数只读，不能修改
{
	return &currentConfig;//返回保存当前参数结构体的地址
}



uint32_t DeviceConfig_GetRevision(void)
{
	return configRevision;//返回配置修订号
}



uint8_t DeviceConfig_Validate(const DeviceConfig_t *config)//设备配置校验
{
	if(config==NULL)
	{
		return 0U;
	}
	
	if(config->TemperatureLimitX100<-4000 ||config->TemperatureLimitX100>12500)
	{
		return 0U;
	}
	if(config->HysteresisX100<1 ||config->HysteresisX100>5000)
	{
		return 0U;
	}
	if((int32_t)config->TemperatureLimitX100 - (int32_t)config->HysteresisX100<-4000)//限制温度 - 回差温度不能小于温度计的下限值
	{
		return 0U;
	}
	if(config->LogIntervalSeconds<1 ||config->LogIntervalSeconds>60)//Log:日志、记录   Interval：间隔  Seconds:秒
	{
		return 0U;
	}
	if(config->SlaveAddress<1 || config->SlaveAddress>247)
	{
		return 0U;
	}
	
	return 1U;
}



uint8_t DeviceConfig_Apply(const DeviceConfig_t *config)
{
	if(DeviceConfig_Validate(config)==0U)
	{
		return 0U;
	}
	
	currentConfig=*config; //修改配置
	configRevision++;      //配置修订号
	
	return 1U;
}



