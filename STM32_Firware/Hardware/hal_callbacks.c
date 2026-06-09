#include "main.h"
#include "uart_protocol.h"
#include "app_wifi.h"

// 统一的 UART 中断分发器
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        UART1_Rx_Handler(); 
    }
    else if (huart->Instance == USART3)
    {
        WiFi_Rx_Handler();  
    }
}