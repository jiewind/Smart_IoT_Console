#include "app_wifi.h"
#include <string.h>
#include <stdio.h>
#include "app_data.h"
#include "uart_protocol.h"  // 引入主指令池相关的全局变量


extern UART_HandleTypeDef huart3;

// 全局变量定义 (保持跨中断变量的 volatile 特性)
volatile char WiFi_Rx_Buf[WIFI_RX_BUF_SIZE] = {0};
volatile uint8_t WiFi_Rx_Flag = 0;
volatile uint16_t wifi_rx_index = 0;
uint8_t wifi_rx_byte; // 单字节接收缓存

// =========================================================
// 🌐 WiFi 账号密码存储库
// =========================================================
static const WiFi_Config_t WiFi_Repository[] = {
    {"OnePlus11",  "12345678"},   
    {"iPhone_Hotspot", "password123"}  
};
#define WIFI_COUNT (sizeof(WiFi_Repository) / sizeof(WiFi_Config_t))

// 状态机状态定义
typedef enum {
    WIFI_STATE_EXIT_TRANS = 0, 
    WIFI_STATE_IDLE,
    WIFI_STATE_SEND_AT,
    WIFI_STATE_CHECK_AUTO,
    WIFI_STATE_SET_MODE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_START_TCP,    
    WIFI_STATE_SET_TRANS,   
    WIFI_STATE_START_SEND,   
    WIFI_STATE_TRANSMITTING, 
    WIFI_STATE_ERROR
} WiFi_State_e;

static WiFi_State_e current_wifi_state = WIFI_STATE_IDLE;
static uint8_t current_wifi_index = 0;

// 辅助函数：查找子串
static uint8_t strsearch(const volatile char* str, const char* target)
{
    return (strstr((const char*)str, target) != NULL) ? 1 : 0;
}

// 发送指令函数
static void WiFi_Send_Cmd(const char* cmd)
{
    memset((void*)WiFi_Rx_Buf, 0, WIFI_RX_BUF_SIZE);
    wifi_rx_index = 0;
    WiFi_Rx_Flag = 0;
    HAL_UART_Transmit(&huart3, (uint8_t*)cmd, strlen(cmd), 100);
}

// 初始化 WiFi
void App_WiFi_Init(void)
{
    current_wifi_state = WIFI_STATE_EXIT_TRANS;
    current_wifi_index = 0;
    HAL_UART_Receive_IT(&huart3, &wifi_rx_byte, 1); // 首次武装捕鼠夹
}

// =========================================================
// 🚀 WiFi 主循环业务状态机 (每 500ms 掐表推进)
// =========================================================
void Task_WiFi_Connection(void)
{
    static uint32_t last_time = 0;
    static uint32_t timeout_counter = 0;
    
    // 状态机核心轮询逻辑
    if (HAL_GetTick() - last_time >= 500)
    {
        last_time = HAL_GetTick();
        
        switch (current_wifi_state)
        {
            case WIFI_STATE_EXIT_TRANS:
                if (timeout_counter == 0) {
                    printf("[WiFi] Escaping Passthrough mode...\r\n");
                    // ⚠️ 注意：绝不能用 WiFi_Send_Cmd，因为它自带 \r\n。必须发纯洁的 +++
                    HAL_UART_Transmit(&huart3, (uint8_t*)"+++", 3, 100);
                }
                
                timeout_counter++;
                // 等待 3 个滴答 (3 * 500ms = 1.5秒)，给 ESP8266 足够的时间退出
                if (timeout_counter > 3) {
                    current_wifi_state = WIFI_STATE_SEND_AT; // 正式开始发 AT
                    timeout_counter = 0;
                }
                break;
            case WIFI_STATE_SEND_AT:
                printf("[WiFi] Checking ESP8266 presence...\r\n");
                WiFi_Send_Cmd("AT\r\n");
                current_wifi_state = WIFI_STATE_SET_MODE;
                timeout_counter = 0;
                break;
                
case WIFI_STATE_CHECK_AUTO:
                // 【新增分支】：查岗逻辑
                if (WiFi_Rx_Flag && strsearch(WiFi_Rx_Buf, "OK")) 
                {
                    printf("[WiFi] Checking auto-connection status...\r\n");
                    WiFi_Send_Cmd("AT+CWJAP?\r\n");
                    current_wifi_state = WIFI_STATE_SET_MODE; // 借用一下，看下一步的回复
                    timeout_counter = 0;
                }
                else 
                {
                    timeout_counter++;
                    if (timeout_counter > 6) current_wifi_state = WIFI_STATE_SEND_AT;
                }
                break;

            case WIFI_STATE_SET_MODE:
                if (WiFi_Rx_Flag) 
                {
                    // 如果回复里包含 "+CWJAP:"，说明它自己已经连上路由器了！
                    if (strsearch(WiFi_Rx_Buf, "+CWJAP:")) 
                    {
                        printf("[WiFi] Auto-connected to Router! Skipping setup.\r\n");
                        current_wifi_state = WIFI_STATE_START_TCP; // 直接跨级去连 TCP！
                        timeout_counter = 0;
                    }
                    // 如果回复 "No AP" 或者其他错误，说明没连上，老老实实配网
                    else if (strsearch(WiFi_Rx_Buf, "No AP") || strsearch(WiFi_Rx_Buf, "OK"))
                    {
                        printf("[WiFi] No auto-connection. Setting Station mode...\r\n");
                        WiFi_Send_Cmd("AT+CWMODE=1\r\n");
                        current_wifi_state = WIFI_STATE_CONNECTING;
                        timeout_counter = 0;
                    }
                    else
                    {
                        WiFi_Rx_Flag = 0;
                        memset((void*)WiFi_Rx_Buf, 0, WIFI_RX_BUF_SIZE);
                        wifi_rx_index = 0;
                    }
                }
                if (timeout_counter > 6) {
                    current_wifi_state = WIFI_STATE_SEND_AT;
                    timeout_counter = 0;
                }
                break;
                
            case WIFI_STATE_CONNECTING:
                if (WiFi_Rx_Flag && strsearch(WiFi_Rx_Buf, "OK"))
                {
                    char connect_cmd[128];
                    sprintf(connect_cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", 
                            WiFi_Repository[current_wifi_index].ssid, 
                            WiFi_Repository[current_wifi_index].password);
                    
                    printf("[WiFi] Attempting to connect to Repo[%d]: %s...\r\n", 
                            current_wifi_index, WiFi_Repository[current_wifi_index].ssid);
                    
                    WiFi_Send_Cmd(connect_cmd);
                    current_wifi_state = WIFI_STATE_CONNECTED;
                    timeout_counter = 0;
                }
                break;
                
            case WIFI_STATE_CONNECTED:
                if (WiFi_Rx_Flag && strsearch(WiFi_Rx_Buf, "OK"))
                {
                    printf("[WiFi] SUCCESS! Connected to Router.\r\n");
                    current_wifi_state = WIFI_STATE_START_TCP; 
                    timeout_counter = 0;
                }
                else if (WiFi_Rx_Flag && (strsearch(WiFi_Rx_Buf, "FAIL") || strsearch(WiFi_Rx_Buf, "ERROR")))
                {
                    printf("[WiFi] Failed to connect to %s\r\n", WiFi_Repository[current_wifi_index].ssid);
                    current_wifi_index = (current_wifi_index + 1) % WIFI_COUNT;
                    current_wifi_state = WIFI_STATE_SEND_AT; 
                }
                else
                {
                    timeout_counter++;
                    if (timeout_counter > 30) {
                        current_wifi_index = (current_wifi_index + 1) % WIFI_COUNT;
                        current_wifi_state = WIFI_STATE_SEND_AT;
                    }
                }
                break;
                
            case WIFI_STATE_START_TCP:
                // ⚠️ 请确保这里的 IP 已经改成了你 Windows 的 ipconfig 真实无线 IP！
                printf("[TCP] Connecting to Server 192.168.31.126:8080...\r\n");
                WiFi_Send_Cmd("AT+CIPSTART=\"TCP\",\"192.168.31.126\",8080\r\n");
                current_wifi_state = WIFI_STATE_SET_TRANS;
                timeout_counter = 0;
                break;
                
            case WIFI_STATE_SET_TRANS:
                timeout_counter++;
                if (WiFi_Rx_Flag)
                {
                    if (strsearch(WiFi_Rx_Buf, "CONNECT"))
                    {
                        printf("[TCP] Server Connected! Setting Passthrough mode...\r\n");
                        WiFi_Send_Cmd("AT+CIPMODE=1\r\n"); 
                        current_wifi_state = WIFI_STATE_START_SEND;
                        timeout_counter = 0;
                    }
                    else if (strsearch(WiFi_Rx_Buf, "ERROR") || strsearch(WiFi_Rx_Buf, "CLOSED"))
                    {
                        printf("[TCP] Connection Failed. Retrying...\r\n");
                        current_wifi_state = WIFI_STATE_START_TCP; 
                        timeout_counter = 0;
                    }
                    else
                    {
                        // 忽略废话消息，擦干净继续等真正的 CONNECT
                        WiFi_Rx_Flag = 0;
                        memset((void*)WiFi_Rx_Buf, 0, WIFI_RX_BUF_SIZE);
                        wifi_rx_index = 0;
                    }
                }
                if (timeout_counter > 40) // 20秒超长等待
                {
                    printf("[TCP] Handshake Timeout! Retrying...\r\n");
                    current_wifi_state = WIFI_STATE_START_TCP;
                    timeout_counter = 0;
                }
                break;
                
            case WIFI_STATE_START_SEND:
                if (WiFi_Rx_Flag && strsearch(WiFi_Rx_Buf, "OK"))
                {
                    printf("[TCP] Entering Transmitting status! Pipe is OPEN.\r\n");
                    WiFi_Send_Cmd("AT+CIPSEND\r\n"); 
                    current_wifi_state = WIFI_STATE_TRANSMITTING;
                    timeout_counter = 0;
                }
                break;
                
            case WIFI_STATE_TRANSMITTING:
                // 【绝妙设计：主循环内部的无线下行指令分流】
                if (WiFi_Rx_Flag == 1)
                {
                    // 只要透传状态下有数据进来，100%是电脑发来的控制指令！
                    // 我们在主循环里安全地把它“灌”进主指令池中
                    memset(Rx_Data_Buf, 0, RX_BUF_SIZE);
                    strncpy(Rx_Data_Buf, (const char*)WiFi_Rx_Buf, RX_BUF_SIZE - 1);
                    Rx_Data_Index = strlen(Rx_Data_Buf);
                    Rx_Flag = 1; // 强行拉高标志位，通知主循环另一侧的流水线去解析开灯！
                    
                    // 清空 Wi-Fi 池，功成身退，准备接下一条
                    WiFi_Rx_Flag = 0;
                    memset((void*)WiFi_Rx_Buf, 0, WIFI_RX_BUF_SIZE);
                    wifi_rx_index = 0;
                }

                // 定时上报温湿度上行逻辑 (每2秒)
                {
                    static uint32_t last_tx_time = 0;
                    if (HAL_GetTick() - last_tx_time >= 2000)
                    {
                        last_tx_time = HAL_GetTick();
                        char tcp_json_buf[128];
                        const char* led_str = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) ? "ON" : "OFF";
                        
                        sprintf(tcp_json_buf, "{\"temp\":%d,\"humi\":%d,\"light\":%d,\"led\":\"%s\", \"page\":%d}\r\n", 
                                g_SysData.temperature, 
                                g_SysData.humidity, 
                                g_SysData.light_percent, 
                                led_str,
                                g_SysData.current_page
                            );
                        
                        HAL_UART_Transmit(&huart3, (uint8_t*)tcp_json_buf, strlen(tcp_json_buf), 50);
                        printf("[TCP Log] Sent JSON via Wi-Fi!\r\n");
                    }
                }
                break;
                
            default:
                break;
        }
    }
}

// =========================================================
// 📥 真正的工业级中断函数：不带任何业务逻辑，只管纯粹、极速地捞数据！
// =========================================================
void WiFi_Rx_Handler(void)
{
    // 强制清除溢出错误，永不死锁
    __HAL_UART_CLEAR_OREFLAG(&huart3);
    
    if (wifi_rx_index < WIFI_RX_BUF_SIZE - 1)
    {
        WiFi_Rx_Buf[wifi_rx_index++] = wifi_rx_byte;
        if (wifi_rx_byte == '\n') {
            WiFi_Rx_Flag = 1; 
        }
    }
    else {
        wifi_rx_index = 0; 
    }
    
    // 以微秒级的速度重新武装捕鼠夹
    HAL_UART_Receive_IT(&huart3, &wifi_rx_byte, 1); 
}