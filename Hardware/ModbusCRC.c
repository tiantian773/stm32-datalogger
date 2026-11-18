#include "ModbusCRC.h"


uint16_t Modbus_CRC16(const uint8_t *data, uint16_t length)//计算CRC校验
{
	uint16_t crc=0xFFFF;//初始值
	uint16_t i;
	uint8_t bit;
	
	for(i=0;i<length;i++)
	{
		crc^=data[i];
		
		for(bit=0;bit<8;bit++)
		{
			if((crc & 0x0001U)!=0U)
			{
				crc >>= 1;
				crc^=0xA001U;
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	
	return crc;
}


uint8_t Modbus_CheckCRC(const uint8_t *frame, uint16_t length)//核对报文CRC校验是否正确
{
	uint16_t calculatedCrc;//计算出的CRC校验
	uint16_t receivedCrc;//报文里的CRC校验值
	
	if(frame==NULL || length<4)
	{
		return 0;
	}
	
	calculatedCrc=Modbus_CRC16(frame,length-2U);
	receivedCrc=((uint16_t)frame[length-2U] | (uint16_t)frame[length-1U]<<8);//将报文的CRC校验值拼成完整值，低位先行 报文的倒数第二个字节就是CRC校验的低8位
	if(receivedCrc==calculatedCrc)//判断计算的CRC值和报文里的CRC值是否相等
	{
		return 1;
	}
	else
	{
		return 0;
	}	
}
