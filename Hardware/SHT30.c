#include "SHT30.h"
#include "i2c.h"

#define SHT30_I2C_TIMEOUT_MS 20U

/**
 * @brief  SHT30_Init：初始化SHT30，启动1Hz周期采集
 * @param  无
 * @return HAL_OK：初始化成功；其他值：通信失败
 */
HAL_StatusTypeDef  SHT30_Init(void)
{
	uint8_t StopCmd[]={0x30,0x93};
	uint8_t StartCmd[]={0x21,0x30};
	
	HAL_StatusTypeDef status;
	
	HAL_Delay(10);//上电等待
	
	status=HAL_I2C_Master_Transmit(&hi2c1,(0x44<<1),StopCmd,2,SHT30_I2C_TIMEOUT_MS);//防止主控断电 SHT30没断电，先停止
	if(status!=HAL_OK)
	{
		return status;//
	}
	HAL_Delay(2);//硬件停止需要1ms左右
	
	status=HAL_I2C_Master_Transmit(&hi2c1,(0x44<<1),StartCmd,2,SHT30_I2C_TIMEOUT_MS);
	
	return status; // 把第二次发送的结果交给调用者
}


/**
 * @brief  SHT30_Read：读取周期采集的6字节原始数据，包含CRC
 * @param  Data：接收缓冲区，至少提供6字节空间
 * @return HAL_OK：读取成功；其他值：通信失败
 * @note   本函数只读取数据，不进行CRC校验和温湿度换算
 */
HAL_StatusTypeDef SHT30_Read(uint8_t *Data)
{
	uint8_t Cmd[]={0xE0,0x00};
	HAL_StatusTypeDef status;
	
	status=HAL_I2C_Master_Transmit(&hi2c1,(0x44<<1),Cmd,sizeof(Cmd),SHT30_I2C_TIMEOUT_MS);
	if(status!=HAL_OK)
	{
		if(status==HAL_BUSY)
		{
			return HAL_ERROR;
		}
		
		return status;
	}
	
	status=HAL_I2C_Master_Receive(&hi2c1,(0x44<<1),Data,6U,SHT30_I2C_TIMEOUT_MS);
	
	/*
    * 命令发送成功，但读取阶段收到 NACK：
    * 可能暂时没有新数据，交给上层稍后重试。
    */
	if(status==HAL_ERROR && HAL_I2C_GetError(&hi2c1)==HAL_I2C_ERROR_AF)
	{
		return HAL_BUSY;
	}
	
	if(status==HAL_BUSY)
	{
		return HAL_ERROR;
	}
	
	return status;
	
}


/**
 * @brief  SHT30_CalCRC：根据SHT30规定的算法计算CRC-8校验值
 * @param  Data：待计算数据的首地址
 * @param  Length：参与计算的字节数，SHT30每组数据传入2
 * @return 计算得到的1字节CRC值
 */
static uint8_t SHT30_CalCRC(const uint8_t *Data,uint8_t Length)
{
	uint8_t crc=0xFF;
	uint8_t i,bit;
	
	for(i=0;i<Length;i++)
	{
		crc^=Data[i];
		
		for(bit=0;bit<8;bit++)
		{
			if(crc & 0x80)
			{
				crc=(uint8_t)((crc<<1)^0x31);
			}
			else
			{
				crc=(uint8_t)(crc<<1);
			}
		}
	}
	
	return crc;
}


/**
 * @brief  SHT30_CheckCRC：分别校验温度、湿度数据的CRC
 * @param  Data：已读取的6字节原始数据
 * @return HAL_OK：两组校验均通过；HAL_ERROR：校验失败
 */
HAL_StatusTypeDef SHT30_CheckCRC(const uint8_t *Data)
{
	if(SHT30_CalCRC(&Data[0],2)!=Data[2])
	{
		return HAL_ERROR;
	}
	
	if(SHT30_CalCRC(&Data[3],2)!=Data[5])
	{
		return HAL_ERROR;
	}
	
	return HAL_OK;
}


/**
 * @brief  SHT30_Convert：读取数据、校验CRC，并换算为实际温湿度
 * @param  temperature：温度输出指针，单位为℃
 * @param  humidity：相对湿度输出指针，单位为%RH
 * @return HAL_OK：成功；其他值：读取或校验失败
 * @note   仅在读取和校验均成功时更新输出值
 */
HAL_StatusTypeDef SHT30_Convert(float *temperature,float *humidity)
{
	uint8_t Data[6];
	uint16_t temp,hum;
	HAL_StatusTypeDef status;
	
	status=SHT30_Read(Data);
	if(status!=HAL_OK)
	{
		return status;
	}
	
	status=SHT30_CheckCRC(Data);
	if(status!=HAL_OK)
	{
		return status;
	}
	
	temp=((uint16_t)Data[0]<<8)| Data[1];
	hum=((uint16_t)Data[3]<<8)| Data[4];
	
	*temperature=-45.0f+(175.0f*(temp/65535.0f));
	*humidity=100.0f*(hum/65535.0f);
	
	return status;
}

