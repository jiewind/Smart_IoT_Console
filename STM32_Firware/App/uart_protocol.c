#include "uart_protocol.h"
#include <string.h>
#include <stdio.h>
#include "app_wifi.h"
#include "app_data.h"

// 外部引用的串口句柄
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;

// 串口缓冲区及相关变量
uint8_t aRxBuffer;                      // 接收中断中的单字节暂存
uint8_t Rx_Data_Idx = 0;                // 池子的当前写入位置

char Rx_Data_Buf[RX_BUF_SIZE] = {0};
volatile uint16_t Rx_Data_Index = 0;
volatile uint8_t Rx_Flag = 0;

// USART1 专属硬件句柄和单字节缓存
extern UART_HandleTypeDef huart1;
uint8_t rx_byte; 

// ==========================================
// USART1 专属接收与重新武装逻辑
// ==========================================
void UART1_Rx_Handler(void)
{
    if (Rx_Data_Index < RX_BUF_SIZE - 1)
    {
        Rx_Data_Buf[Rx_Data_Index++] = rx_byte;
        if (rx_byte == '\n') {
            Rx_Flag = 1;
        }
    }
    else {
        Rx_Data_Index = 0;
    }
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1); 
}

// 初始化：开启第一次中断监听
void UART_Protocol_Init(void) {
    HAL_UART_Receive_IT(&huart1, &aRxBuffer, 1);
}


// 指令解析函数：在主循环中被调用
void UART_Protocol_Process(void) {
    if (Rx_Flag == 1) {
        printf("[UART] Received CMD: %s\r\n", Rx_Data_Buf);

        // 使用 strstr 包含完整单词
        if (strstr(Rx_Data_Buf, "open") != NULL) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // 开灯
            printf("[ACK] LED is OPEN now!\r\n");
        } 
        else if (strstr(Rx_Data_Buf, "close") != NULL) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   // 关灯
            printf("[ACK] LED is CLOSED now!\r\n");
        } 
        else if (strstr(Rx_Data_Buf, "page1") != NULL) {
            g_SysData.current_page = PAGE_HOME;
            printf("[ACK] Switched to HOME page!\r\n");
        } 
        else if (strstr(Rx_Data_Buf, "page2") != NULL) {
            g_SysData.current_page = PAGE_SYSTEM;
            printf("[ACK] Switched to SYSTEM page!\r\n");
        } 
        else if (strstr(Rx_Data_Buf, "status") != NULL) 
        {
            // 1. 准备一个足够大的数组来装完整的 JSON 句子
            char json_buf[128]; 
            
            // 2. 读取当前 LED 的真实物理状态
            const char* led_str = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) ? "ON" : "OFF";
            
            // 3. 【核心组装】使用 sprintf 和转义字符 \" 拼接 JSON
            // 注意：在 C 语言的字符串里想打出一个双引号 "，必须写成 \"
            sprintf(json_buf, "{\"temp\":%d, \"humi\":%d, \"light\":%d, \"led\":\"%s\", \"page\":%d}\r\n", 
                    g_SysData.temperature, 
                    g_SysData.humidity, 
                    g_SysData.light_percent,
                    led_str, 
                    g_SysData.current_page);
            
            // 4. 一次性将组装好的 JSON 发送给电脑
            printf("%s", json_buf);
        }
        else {
            printf("[ERR] Unknown command. Please use 'open', 'close', 'page1', or 'page2'\r\n");
        }

        // 解析完成，重置池子，准备迎接下一条指令
        Rx_Data_Idx = 0;
        Rx_Flag = 0;
        memset(Rx_Data_Buf, 0, UART_RX_BUF_SIZE);
    }
}
