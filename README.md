# STM32 数据采集与 Modbus 通信终端

基于 STM32F103C8 与 STM32 HAL 的裸机数据采集终端，实现温湿度及双路模拟量采集、OLED 显示、温度回差报警、RS485 Modbus 通信、SPI Flash 循环日志与参数保存。

当前代码为 **裸机版 v1.0**，不使用 RTOS。系统在主循环中按时间片检查周期任务，通过中断、DMA 与环形缓冲区处理采样及串口接收；参数保存采用状态推进方式。

## 功能

- SHT30 温湿度读取和 CRC-8 校验；主循环每秒检查采集，包含传感器故障与重新初始化路径。
- TIM3 触发 ADC1 双通道扫描，DMA 循环搬运，累计窗口均值。
- OLED 按 200 ms 间隔检查刷新；温度报警支持阈值、回差和蜂鸣器控制。
- RS485 半双工 Modbus RTU 从站，支持 0x03、0x04、0x06、0x10 功能码，以及按序号读取历史记录的私有扩展 0x41。
- W25Q32 循环日志，32 字节记录槽，包含 CRC、提交标记、写后回读和启动恢复逻辑。
- A/B 双区配置保存；支持修改报警阈值、回差、日志间隔及从站地址，并显式保存。

以上说明来自源码实现，周期为配置目标；实测性能与稳定性应结合硬件验证。当前仓库暂未附测试报告、实物照片和演示视频。

## 默认参数

| 参数 | 默认值 |
| --- | --- |
| MCU | STM32F103C8 |
| 外部 Flash | W25Q32，启动检查 JEDEC ID EF 40 16 |
| Modbus 从站地址 | 1 |
| 串口 | USART1，115200 bit/s，8N1 |
| 温度上限 / 回差 | 31.00 ℃ / 1.00 ℃ |
| 日志间隔 | 1 秒，可配置 1～60 秒 |

Flash 中已有有效配置时，启动恢复保存值，可能不同于上表。

## 工程目录

```text
Core/                         主循环、外设初始化和中断
Hardware/                     传感器、OLED、Flash、日志和 Modbus 模块
Drivers/                      STM32 HAL 与 CMSIS 依赖
MDK-ARM/                      Keil 工程及启动文件
STM32_DataLogger_F103.ioc      CubeMX 配置
```

阅读入口为 `Core/Src/main.c`。日志和配置实现分别位于 `Hardware/DataLogger.c`、`Hardware/ConfigStore.c`，协议处理位于 `Hardware/ModbusSlave.c` 和 `Hardware/ModbusRegisters.c`。

## 编译与下载

1. 安装 Keil MDK 和 STM32F1 设备包。现有工程记录 ARM Compiler 5.06 update 5（build 528）及 Keil.STM32F1xx_DFP.2.2.0，这是原工程元数据。
2. 打开 `MDK-ARM/STM32_DataLogger_F103.uvprojx`。
3. 确认器件、编译器和 SWD 调试器设置与本机及实物一致，执行 Rebuild。
4. 下载到开发板，连接外设后，用 115200 / 8N1、从站地址 1 进行上位机联调。

本次整理已核对工程源文件与 IncludePath 目录是否存在，未在此环境执行 Keil 编译或硬件测试。

## 主要引脚

| 功能 | 引脚 |
| --- | --- |
| I²C1，SHT30 / OLED | PB6 SCL、PB7 SDA |
| SPI1，W25Q32 | PA5 SCK、PA6 MISO、PA7 MOSI、PA4 CS |
| USART1，RS485 | PA9 TX、PA10 RX、PA11 收发方向 |
| ADC1 双路输入 | PB0 通道8、PB1 通道9 |
| 蜂鸣器控制 | PB11，当前代码低电平报警 |
| SWD | PA13 SWDIO、PA14 SWCLK |

引脚表由源码配置整理，接线需核对实际板卡和模块；外设共地，模拟输入限制在 0～VDDA。

## 版本说明

本版本作为裸机实现保留，后续可继续提交 FreeRTOS 版本，用提交记录和版本标签展示演进。

本次整理保留原始源码字节内容，移除了编译输出、本机界面状态，以及本工程未引用的 CMSIS DSP/NN 示例和文档。第三方组件及源文件中的版权、许可声明予以保留；实际复用的 OLED、字库和驱动来源可在核实后补充。未另加全仓库统一许可证。
