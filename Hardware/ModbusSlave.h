#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

#include <stdint.h>

uint16_t ModbusSlave_ProcessFrame(
    const uint8_t *frame,      // 收到的请求帧
    uint16_t frameLength,      // 请求帧实际有多少字节
    uint8_t *response,         // 将生成的回复写到哪里
    uint16_t responseCapacity  // 回复缓冲区最多能容纳多少字节
);

#endif
