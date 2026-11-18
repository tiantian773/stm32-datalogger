#include "RingBuffer.h"

#define RING_BUFFER_SIZE 512U

static uint16_t buffer[RING_BUFFER_SIZE];//环形队列，用来存放串口接收到的数据
static volatile uint16_t writeIndex=0;//写索引，指向第一个空位置
static volatile uint16_t readIndex=0;//读索引

void RingBuffer_Init(void)
{
	writeIndex=0;
	readIndex=0;
}

uint8_t RingBuffer_Push(uint16_t Data)//入队
{
	uint16_t nextWrite;
	
	nextWrite=(writeIndex+1)%RING_BUFFER_SIZE;
	if(nextWrite==readIndex)
	{
		return 0;//缓冲区已满
		
		
	}
	
	buffer[writeIndex]=Data;
	writeIndex=nextWrite;
	
	return 1;
}

uint8_t RingBuffer_Pop(uint16_t *data)
{
	if(data==NULL)
	{
		return 0;//空指针
	}
	
	if(readIndex==writeIndex)//读索引等于写索引
	{
		return 0;//缓冲区为空
	}
	
	*data=buffer[readIndex];//获取一个元素
	readIndex=(readIndex+1)%RING_BUFFER_SIZE;
	
	return 1;//取出成功
}
