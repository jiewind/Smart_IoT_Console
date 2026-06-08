#ifndef __UART_PROTOCOL_H
#define __UART_PROTOCOL_H

#include "main.h"

// ==========================================
// 补齐丢失的宏定义 (两个名字都写上，双重保险)
// ==========================================
#define RX_BUF_SIZE 128
#define UART_RX_BUF_SIZE 128 

// ==========================================
// 补齐丢失的外部变量声明 (加上 volatile 防优化)
// ==========================================
extern char Rx_Data_Buf[RX_BUF_SIZE];
extern volatile uint16_t Rx_Data_Index;
extern volatile uint8_t Rx_Flag;

// 核心功能函数声明
void UART_Protocol_Init(void);
void UART_Protocol_Process(void);

// 新增的 USART1 专属处理函数声明
void UART1_Rx_Handler(void);

#endif