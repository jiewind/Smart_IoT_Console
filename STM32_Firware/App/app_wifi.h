#ifndef __APP_WIFI_H
#define __APP_WIFI_H

#include "main.h"

// 定义单个 WiFi 信息的结构体
typedef struct {
    const char* ssid;     // WiFi 账号
    const char* password; // WiFi 密码
} WiFi_Config_t;

// 声明初始化和任务函数
void App_WiFi_Init(void);
void Task_WiFi_Connection(void);

// 供串口3中断回调使用的接收缓冲区声明
#define WIFI_RX_BUF_SIZE 256
extern volatile char WiFi_Rx_Buf[WIFI_RX_BUF_SIZE];
extern volatile uint8_t WiFi_Rx_Flag;

// 声明 USART3 专属的中断处理函数
void WiFi_Rx_Handler(void);

#endif