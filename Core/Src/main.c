/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include "W25Q32.h"
#include "SHT30.h"
#include "AnalogInput.h"
#include "DataLogger.h"
#include "TempAlarm.h"
#include "RingBuffer.h"
#include "ModbusCRC.h"
#include "DeviceConfig.h"
#include "ModbusSlave.h"
#include "ModbusRegisters.h"
#include "ConfigStore.h"





/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

DataLogger_Record_t writeRecord ={0};        //整理本次准备写入 Flash 的日志数据        

HAL_StatusTypeDef initStatus = HAL_BUSY;     //判断日志模块初始化是否成功
HAL_StatusTypeDef adcStartStatus = HAL_BUSY; //判断 ADC 启动是否成功
HAL_StatusTypeDef adcStatus = HAL_BUSY;      //判断 ADC 数据能否用于本次发布和记录
TempAlarm_State_t tempAlarmState = TEMP_ALARM_STATE_NORMAL;//决定报警状态位、OLED 的 ALM 和蜂鸣器输出
uint8_t rxByte =0;          //HAL 单字节接收的存放位置
uint8_t frameBuffer[256];   //保存正在拼接的帧,不包含结束标记
uint16_t frameLength = 0;   //帧长度
uint8_t frameDiscard = 0;   //决定当前帧是否继续处理
volatile uint8_t rxFrameResetRequest=0U;     //通知主循环清掉正在拼接的旧帧
volatile uint8_t rxDiscardUntilGap=0U;       //决定是否丢弃字节，等待帧间隔
volatile uint8_t uartRxRestartRequest=0U;    //通知主循环重试开启接收
uint8_t modbusTxBuff[256];                   //保存准备发送的应答
HAL_StatusTypeDef modbusTxStatus=HAL_OK;     //判断发送成功还是失败，更新对应计数
HAL_StatusTypeDef configLoadStatus=HAL_BUSY; //判断配置加载是否成功



HAL_StatusTypeDef logStatus = HAL_BUSY;                       //保存最近一次追加日志的返回结果，后面没有根据它进行判断
DataLogger_State_t loggerState = DATA_LOGGER_STATE_NOT_READY; //保存日志模块状态的副本，主要用于 Watch 观察
HAL_StatusTypeDef formatStatus=HAL_BUSY;                      //保存清空日志的返回结果，后面没有根据它进行判断


volatile uint32_t rxCount = 0;       //接收字节数量
uint32_t popCount=0;                 //成功从队列取出字节个数
volatile uint32_t rxDropCount=0;     //软件主动丢弃、没有入队的字节数
volatile uint32_t rxFrameEndCount=0; // 接收结束通知次数
uint32_t frameLengthErrorCount = 0;  //帧长度异常次数

/* 调试统计 */
volatile uint32_t uartErrorCount=0U;         //串口错误计数
volatile uint32_t uartRxRestartFailCount=0U; //串口重启失败
volatile uint32_t rxQueueOverFlowCount=0U;   //队列溢出计数
volatile uint32_t rxResyncCount=0U;          //重新同步

uint32_t crcPassCount = 0;  //CRC校验成功数量
uint32_t crcFailCount = 0;  //CRC校验失败数量

uint32_t modbusTxCount=0U;
uint32_t modbusTxErrorCount=0U;


volatile uint32_t uartLastError=HAL_UART_ERROR_NONE; //最近一次 UART 错误码，供调试观察，恢复后仍保留
volatile uint8_t clearLogRequest=0U;//主动清空请求：置 1 后执行一次

HAL_StatusTypeDef flashIdStatus=HAL_BUSY;
uint8_t flashManufacturerID=0U; //厂家ID
uint16_t flashDeviceID=0U;      //设备ID
uint8_t storageFault=1U;        // 初始化成功后才清零
uint32_t storageErrorCount=0U;  // 存储故障计数

volatile uint32_t logDropCount=0U;//记录丢弃

HAL_StatusTypeDef sht30InitStatus=HAL_BUSY;
HAL_StatusTypeDef sht30Status=HAL_BUSY;

uint8_t sht30Fault=1U;        // 取得有效数据后才清零
uint8_t sht30NeedInit=1U;     // 是否需要重新初始化
uint8_t sht30NoDataCount=0U;  // 连续没有新数据的次数

uint32_t sht30ErrorCount=0;   // 确认异常、初始化失败的累计次数
uint32_t sht30RetryCount=0U;  // 重试初始化次数，不包含开机那次



/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void Modbus_RestartGapTimer(void)//重新开始帧间隔计时。
{
	HAL_TIM_Base_Stop_IT(&htim2);
	
	__HAL_TIM_SET_COUNTER(&htim2,0U); //设置计数单元
	__HAL_TIM_CLEAR_FLAG(&htim2,TIM_FLAG_UPDATE);//清除溢出标志位
	
	if(HAL_TIM_Base_Start_IT(&htim2)!=HAL_OK)//打开定时器2以及中断
	{
		Error_Handler();
	}
}

static void Modbus_ResetRxFrame(uint8_t waitForGap)//清空接收队列，通知主循环放弃旧帧。
{
	RingBuffer_Init();//初始化接收队列
	
	rxFrameResetRequest=1U;        //帧重新设置请求
	rxDiscardUntilGap=waitForGap;  // 设置“丢弃数据，直到出现帧间隔”的标志
	
	rxResyncCount++;               // 重新同步的次数加1
}
	
static void Modbus_ArmRx(void)//开启单字节接收，失败时留下重试请求。
{
	if(HAL_UART_Receive_IT(&huart1,&rxByte,1U)==HAL_OK)
	{
		uartRxRestartRequest=0U;//将重启请求标志位恢复
	}
	else
	{
		uartRxRestartFailCount++;//重启失败次数增加
		uartRxRestartRequest=1U;
		
		Modbus_ResetRxFrame(1U);
		Modbus_RestartGapTimer();
	}
}

static void Modbus_ProcessRxRestart(void)//主循环处理接收重启请求。
{
	uint32_t irqState;
	
	if(uartRxRestartRequest==0U)
	{
		return;
	}
	
	irqState=__get_PRIMASK();//获取中断状态
	__disable_irq();         //暂停中断
	
	if(uartRxRestartRequest!=0)
	{
		if(huart1.RxState==HAL_UART_STATE_READY)
		{
			__HAL_UART_CLEAR_OREFLAG(&huart1);//清除 USART1 的接收溢出错误标志
			
			Modbus_ResetRxFrame(1U);
			Modbus_RestartGapTimer();
			Modbus_ArmRx();
		}
		else if(huart1.RxState==HAL_UART_STATE_BUSY_RX)
		{
			/* 已经在等待接收，不需要重复开启 */
			uartRxRestartRequest=0U;
		}
	}
	
	__set_PRIMASK(irqState);
}


static HAL_StatusTypeDef SHT30_ReadWithRecovery(float *temperature,float *humidity)//带恢复机制的读取
{
	HAL_StatusTypeDef status;
	
	/* 需要初始化：本轮只尝试初始化 */
	if(sht30NeedInit!=0U)
	{
		sht30RetryCount++;
		
		sht30InitStatus=SHT30_Init();
		if(sht30InitStatus==HAL_OK)
		{
			sht30NeedInit=0U;
			sht30NoDataCount=0U;
			
			/* 已启动测量，下一秒再读取 */
			return HAL_BUSY;
		}
		
		sht30Fault=1U;
		sht30ErrorCount++;
		
		return HAL_ERROR;
	}
	
	/* 已启动测量：尝试读取并校验 */
	status=SHT30_Convert(temperature,humidity);
	if(status==HAL_OK)
	{
		sht30Fault=0U;
		sht30NoDataCount=0U;
		
		return HAL_OK;
	}
	
	if(status==HAL_BUSY)
	{
		sht30NoDataCount++;
		
		/* 第一次没有新数据，先等待下一秒 */
		if(sht30NoDataCount<2U)
		{
			return HAL_BUSY;
		}
		
		/* 连续两次没有数据，转入异常恢复 */
		status=HAL_ERROR;
	}
	
	/* 通信、CRC 或持续无数据异常 */
	sht30Fault=1U;
	sht30ErrorCount++;
	sht30NeedInit=1U;
	
	return status;
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  
  OLED_Init();
  
  sht30InitStatus=SHT30_Init();
  {
	  if(sht30InitStatus==HAL_OK)
	  {
		  sht30NeedInit=0U;
	  }
	  else
	  {
		  sht30Status=HAL_ERROR;
		  sht30ErrorCount++;
	  }	  
  }
  
  W25Q32_Init();
  
  DeviceConfig_Init();//先建立默认配置
  
  storageFault=1U;
  flashIdStatus=W25Q32_ReadID(&flashManufacturerID,&flashDeviceID);
  if(flashIdStatus==HAL_OK && flashManufacturerID==0xEFU && flashDeviceID==0x4016U)
  {
	  /* 芯片识别正确，再读取配置 */
	  configLoadStatus=ConfigStore_Init();
	  if(configLoadStatus==HAL_OK)
	  {
		  /* 配置读取流程完成，再恢复日志 */
		  initStatus=DataLogger_Init();
		  if(initStatus==HAL_OK)
		  {
			   storageFault=0U;
		  }	 
	  }  
  }
  if(storageFault!=0U)
  {
	  storageErrorCount++;
	  ConfigStore_StopOnFault();
  }
	  
  ModbusRegisters_Init();
  TempAlarm_Init();
  RingBuffer_Init();
   
  
  
  OLED_ShowText(1,1,"Temp:     ℃");
  OLED_ShowText(2,1,"RH:     ％");  
  OLED_ShowText(3,1,"V1:    V");
  OLED_ShowText(4,1,"V2:    V");
 
  HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11,GPIO_PIN_RESET);    //拉低MAX3485EN引脚切换到接收状态
  __HAL_UART_CLEAR_OREFLAG(&huart1);                      //清除 USART1 的接收溢出标志 ORE。                  
  if (HAL_UART_Receive_IT(&huart1, &rxByte, 1) != HAL_OK) //先手动打开一次中断接收函数
  {
      Error_Handler();
  }
    
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  float temperature = 0.0f;
  float humidity = 0.0f;
  float Voltage1,Voltage2;
  

  uint32_t last_tick = HAL_GetTick();      // 获取数据采集起始时间
  uint32_t lastDisplayTick=HAL_GetTick();  //获取OLED刷新起始时间
  
  
  const DeviceConfig_t *config=DeviceConfig_Get();//获取当前设备配置
  uint32_t lastLogTick=HAL_GetTick();//计时写入FLASH时间
  uint16_t activeLogIntervalSeconds=config->LogIntervalSeconds;//获取保存间隔
  
  ModbusInputData_t inputData={0}; //输入寄存器变量,用来整理本次采样结果，整理完成后交给寄存器模块。
  
  adcStartStatus=AnalogInput_Start(); //启动ADC
  if(adcStartStatus!=HAL_OK)
  {
	  Error_Handler();//当发生错误时，将执行此函数。
  }
  
  
  
  while (1)
  {
	  if(storageFault==0U && (DataLogger_GetState()==DATA_LOGGER_STATE_ERROR || ConfigStore_GetState()==CONFIG_STORE_ERROR))
	  {
		  storageFault=1U;
		  storageErrorCount++;
		  ConfigStore_StopOnFault();//将存储模块状态更改为错误
	  }
	  
	  if(clearLogRequest!=0 && storageFault==0U && ConfigStore_IsBusy()==0U)
	  {
		  clearLogRequest=0;//清除标志位
		  
		  formatStatus=DataLogger_Format();//按扇区格式化日志区
		  loggerState=DataLogger_GetState();//更新存储模块状态
		  
		  last_tick = HAL_GetTick();//清空完成后重新开始采集计时	
		  lastLogTick=last_tick;
	  }	  
	  
	  
	  uint32_t now = HAL_GetTick();
	  
	  if(activeLogIntervalSeconds!=config->LogIntervalSeconds)
	  {
		  activeLogIntervalSeconds = config->LogIntervalSeconds;//获取新的保存间隔
          lastLogTick = now;		  
	  }
	  
       if((uint32_t)(now-last_tick)>=1000U)
	   {
		   last_tick=now;
		   
		   /* 每轮重新判断哪些数据有效 */
		   inputData.ValidMask=0U;
		   inputData.Status=0U;
		   
		    /* 获取温湿度 */
		   sht30Status=SHT30_ReadWithRecovery(&temperature,&humidity);
		   if(sht30Status==HAL_OK)
		   {
			   inputData.TemperatureX100=(int16_t)(temperature*100.f+(temperature>=0.0f?+0.5f:-0.5f));
			   inputData.HumidityX100=(uint16_t)(humidity*100.f+0.5f);
			   
			   inputData.ValidMask |=MODBUS_VALID_TEMPERATURE | MODBUS_VALID_HUMIDIY;
			   
			   TempAlarm_Update(temperature);//根据温度更新警告标志位
			   tempAlarmState=TempAlarm_GetState();//获取最新温度警告标志位   
		   }
		   
		   /* 获取两路 ADC，与温湿度获取结果独立 */
		   adcStatus=AnalogInput_GetAverageRaw(&writeRecord.Analog1,&writeRecord.Analog2);
		   if(adcStatus==HAL_OK)
		   {
			   inputData.Analog1mV=(uint16_t)(((uint32_t)writeRecord.Analog1*3300U+2047)/4095U);
			   inputData.Analog2mV=(uint16_t)(((uint32_t)writeRecord.Analog2*3300U+2047)/4095U);
			   
			   inputData.ValidMask |=MODBUS_VALID_ANALOG1 | MODBUS_VALID_ANALOG2;
		   }
		   
		   /* 整理本次设备状态 */
		   if(sht30Status==HAL_OK && tempAlarmState==TEMP_ALARM_STATE_HIGH)
		   {
			   inputData.Status |=MODBUS_STATUS_ALARM;
		   }
		   if(storageFault==0U && DataLogger_GetState()==DATA_LOGGER_STATE_READY)
		   {
			   inputData.Status |=MODBUS_STATUS_LOG_READY;
		   }
		   if(storageFault!=0U)
		   {
			   inputData.Status |=MODBUS_STATUS_STORAGE_FAULT;
		   }
		   if(sht30Fault!=0)
		   {
			   inputData.Status |=MODBUS_STATUS_SENSOR_FAULT;
		   }
		   
		   /* 先发布本次采样结果 */
		   ModbusRegisters_UpdateInput(&inputData);//将当前采集的数据更新到输入寄存器
		   
		   if(sht30Status==HAL_OK && adcStatus==HAL_OK)
		   {
			   uint32_t logIntervalMs;//记录间隔
			   
			   writeRecord.TimeMs=HAL_GetTick();//开机后运行时间
			   writeRecord.TemperatureX100=inputData.TemperatureX100;
			   writeRecord.HumidityX100=inputData.HumidityX100;
			   
			   logIntervalMs=(uint32_t)activeLogIntervalSeconds*1000U;//将当前配置的保存间隔由s转换成ms
			   if((uint32_t)(now - lastLogTick)>=logIntervalMs && ConfigStore_IsBusy()==0U)//当前时间减去上次记录时间，判断是否达到设置的间隔
			   {
				   lastLogTick=now;
				   if(storageFault==0U && DataLogger_GetState()==DATA_LOGGER_STATE_READY)
				   {
					   logStatus=DataLogger_AppendRecord(&writeRecord);//将数据记录进FLASH
					   if(logStatus!=HAL_OK)
					   {
						   logDropCount++;
						   
						   storageFault=1U;
						   storageErrorCount++;
						   ConfigStore_StopOnFault();
					   }
				   }
				   else
				   {
				       logDropCount++;
				   }
			   }   
		   }  
	   }
      
	   
	   
	   loggerState = DataLogger_GetState();//记录器状态
	   
	   
	       uint32_t displayNow =HAL_GetTick();
	   
	       if((displayNow - lastDisplayTick)>=200U)
	       {
		       lastDisplayTick = displayNow;//重置初始时间
			   
			   OLED_BeginUpdate();
		  
		       if (sht30Status == HAL_OK)
               {
                   OLED_ShowFloat(1, 6, temperature, 5, 2);
                   OLED_ShowFloat(2, 4, humidity, 5, 2);
                   OLED_ShowString(2, 14, "   ");
               }
               else 
               {
				   /* 覆盖旧数值，避免被当成当前有效数据 */
				   OLED_ShowString(1,6,"-----");
				   OLED_ShowString(2,4,"-----");
				   
				   if(sht30Status==HAL_BUSY)
				   {
					   OLED_ShowString(2, 14, "---"); // 尚未就绪
				   }
                   else
				   {
					   OLED_ShowString(2, 14, "ERR"); // 读取失败，保留原来的数值
				   }                
               }
		  
		       if(AnalogInput_GetVoltage(&Voltage1,&Voltage2)==HAL_OK)//判断获取两路ADC电压值是否正常
	           {
		           OLED_ShowFloat(3,4,Voltage1,4,2);
                   OLED_ShowFloat(4,4,Voltage2,4,2);
	           }
			   
			   if(storageFault!=0U)//判断日志是否异常
			   {
				   OLED_ShowString(3,13,"FERR");
			   }
			   else
			   {
				   OLED_ShowString(3,13,"    ");
			   }
		       
		       if(sht30Status==HAL_OK && tempAlarmState==TEMP_ALARM_STATE_HIGH)//判断温度警告状态位
		       {
			       OLED_ShowString(1, 14, "ALM");
			       HAL_GPIO_WritePin(GPIOB,GPIO_PIN_11,GPIO_PIN_RESET);
		       }
		       else
		       {
			        OLED_ShowString(1, 14, "   ");
			        HAL_GPIO_WritePin(GPIOB,GPIO_PIN_11,GPIO_PIN_SET);
		       }		   
	        }
		    
		uint16_t rxBudget;
		Modbus_ProcessRxRestart();
		
		/* 每轮最多处理64个队列元素 */
		for(rxBudget=0U;rxBudget<64U;rxBudget++)
		{
			uint16_t Data;
			uint8_t popStatus;
			uint32_t irqState;
			
			irqState= __get_PRIMASK();//获取中断状态
			
			
			__disable_irq();//关闭中断
			
			if(rxFrameResetRequest!=0U)//检查是否需要放弃旧帧，重置拼帧状态,1时为需要
			{
				rxFrameResetRequest=0U;
				
				
				frameLength=0U;
				frameDiscard=0U;
			}
			
            popStatus=RingBuffer_Pop(&Data);//读取队列，获取一个元素
			__set_PRIMASK(irqState);//恢复原来的状态
			if(popStatus==0U)
			{
				break;
			}
			
			
			if(popStatus==1)//判断有没有读取成功
			{
				if(Data==RING_BUFFER_FRAME_END)//判断读到的是不是帧末尾
				{
					if(frameDiscard==0U)//判断是不是丢弃帧
					{
						if(frameLength<4U)//Modbus RTU 帧包括，从站地址 功能码  数据区 CRC校验(占两个字节)，最少4字节
						{
							frameLengthErrorCount++;// 长度异常次数
						}
						else if(Modbus_CheckCRC(frameBuffer,frameLength)==1U)//计算读取帧的CRC校验值
						{
							uint16_t responseLength;//应答帧长度
							crcPassCount++;	//校验通过
							
							responseLength=ModbusSlave_ProcessFrame(frameBuffer,frameLength,modbusTxBuff,sizeof(modbusTxBuff));//设置参数
							if(responseLength>0U)
							{
								HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11,GPIO_PIN_SET);
								modbusTxStatus=HAL_UART_Transmit(&huart1,modbusTxBuff,responseLength,100U);//回复
								HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11,GPIO_PIN_RESET);
								
								if(modbusTxStatus==HAL_OK)
								{
									modbusTxCount++;
								}
								else
								{
									modbusTxErrorCount++;
								}
							}
						}
						else
						{
							crcFailCount++;//校验不通过
						}
					}
					
					/* 准备下一帧 */
                    frameLength = 0U;
                    frameDiscard = 0U;						
				 }
			     else
			     {
			     	popCount++;//读取数+1
			     	
			     	if(frameDiscard==0U)
					{
						if(frameLength<sizeof(frameBuffer))
						{
							frameBuffer[frameLength]=(uint8_t)Data;//将读到的字节放到到数组里拼成一帧
							frameLength++;
						}
						else
						{
							frameDiscard=1U;
							frameLengthErrorCount++;//错误长度+1
						}
					} 	
			     }
			 } 
		 }
			
	       
	   
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	ConfigStore_Process();			 
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)//串口接收中断回调函数
{
	if(huart!=&huart1)
	{
		return;
	}
	
	rxCount++;
	Modbus_RestartGapTimer();
	
	/*
    * 必须在重新开启接收之前记录错误。
    * HAL_UART_Receive_IT() 会清除 ErrorCode。
    */
	if(huart->ErrorCode!=HAL_UART_ERROR_NONE)
	{
		uartLastError=huart->ErrorCode;
		uartErrorCount++;
		
		rxDropCount++;
		Modbus_ResetRxFrame(1U);
	}
	else if(rxDiscardUntilGap!=0U)
	{
		/* 受损帧尚未结束，继续丢弃 */
		rxDropCount++;//记录软件主动丢弃、没有入队的字节。
	}
	else if(RingBuffer_Push(rxByte)==0)
	{
		rxDropCount++;
		rxQueueOverFlowCount++;
		
		Modbus_ResetRxFrame(1);
	}
	
	Modbus_ArmRx();
	
}


void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if(huart!=&huart1 || huart->ErrorCode==HAL_UART_ERROR_NONE)
	{
		return;
	}
	
	uartLastError=huart->ErrorCode;//记录串口错误
	uartErrorCount++;//串口错误+1
	
	Modbus_ResetRxFrame(1U);
	Modbus_RestartGapTimer();
	__HAL_UART_CLEAR_OREFLAG(huart);
	
	 /*
     * 溢出错误可能使 HAL 停止接收。
     * 只有已经停止时，才重新开启。
     */
	if(huart->RxState==HAL_UART_STATE_READY)
	{
		Modbus_ArmRx();//开启单字节接收
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)//定时器定时中断回调函数,这里用来判断帧结束
{
	if(htim!=&htim2)
	{
		return;
	}
	
	HAL_TIM_Base_Stop_IT(&htim2);
	rxFrameEndCount++;//帧结束+1
	
	if(rxDiscardUntilGap!=0U)
	{
		/*
         * 已经等到受损帧结束。
         * 队列此前已经清空，下一帧可以正常接收。
         */
		rxDiscardUntilGap=0U;
		return;
	}
	
	if(RingBuffer_Push(RING_BUFFER_FRAME_END)==0U)
	{
		rxQueueOverFlowCount++;
		
		/*
         * 此时已经出现帧间隔，
         * 清空队列后可以直接接收下一帧。
         */
		Modbus_ResetRxFrame(0U);
	}	
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
