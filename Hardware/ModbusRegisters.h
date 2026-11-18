#ifndef MODBUS_REGISTERS_H
#define MODBUS_REGISTERS_H

#include <stdint.h>

#define MODBUS_HOLDING_COUNT 5U
#define MODBUS_INPUT_COUNT   8U


typedef enum
{
	MODBUS_REG_OK=0,                 //成功
	MODBUS_REG_ILLEGAL_ADDRESS=2,    //地址或地址范围不存在
	MODBUS_REG_ILLEGAL_VALUE=3,      //数量或参数值不合法
	MODBUS_REG_DEVICE_FAILURE=4,     //	设备执行操作失败
	MODBUS_REG_DEVICE_BUSY=6         //设备正在忙，暂时不能接受请求
}ModbusRegResult_t;

ModbusRegResult_t ModbusRegisters_ReadHolding(uint16_t startAddress,uint16_t quantity,uint16_t *values);
ModbusRegResult_t ModbusRegisters_WriteHolding(uint16_t startAddress,uint16_t quantity,const uint16_t *values);

/* 数据有效标志 */
#define MODBUS_VALID_TEMPERATURE 0x0001U
#define MODBUS_VALID_HUMIDIY     0X0002U
#define MODBUS_VALID_ANALOG1     0X0004U
#define MODBUS_VALID_ANALOG2     0X0008U

/* 设备状态 */
#define MODBUS_STATUS_ALARM      0X0001U
#define MODBUS_STATUS_LOG_READY  0X0002U
#define MODBUS_STATUS_STORAGE_FAULT 0x0004U   //bit2:存储故障
#define MODBUS_STATUS_SENSOR_FAULT  0x0008U   //bit3:传感器故障

/* 保存最近一次采集的数据 */
typedef struct
{
	int16_t TemperatureX100;
	uint16_t HumidityX100;
	
	uint16_t Analog1mV;
	uint16_t Analog2mV;
	
	uint16_t Status;//设备状态
	uint16_t ValidMask;//数据有效标志位
}ModbusInputData_t;

void ModbusRegisters_Init(void);
void ModbusRegisters_UpdateInput(const ModbusInputData_t *data);
ModbusRegResult_t ModbusRegisters_ReadInput(uint16_t startAddress,uint16_t quantity,uint16_t *values);


#endif

