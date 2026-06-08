#include "app_key.h"
#include "app_data.h" // 需要操控页面，引入数据中枢
#include <stdio.h>

void Task_Key_Scan(void)
{
    static uint32_t last_time = 0;
    
    // 记录上一次的按键状态 (默认未按下时是高电平 1)
    static uint8_t last_k2 = 1;
    static uint8_t last_k3 = 1;
    static uint8_t last_k4 = 1;

    // 核心：20ms 时间片轮询 (完美避开 10ms 的物理抖动)
    if (HAL_GetTick() - last_time >= 20)
    {
        last_time = HAL_GetTick();

        // 1. 读取当前的引脚真实物理电平
        uint8_t curr_k2 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2);
        uint8_t curr_k3 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3);
        uint8_t curr_k4 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4);

        // 2. 下降沿检测 (上一次是 1，这一次是 0，说明被结结实实地按下了)
        
        // ------------------ PA2 逻辑：翻转 LED ------------------
        if (curr_k2 == 0 && last_k2 == 1) {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            printf("[KEY] PA2 Pressed: LED Toggled\r\n");
        }
        
        // ------------------ PA3 逻辑：切换到主页 ------------------
        if (curr_k3 == 0 && last_k3 == 1) {
            g_SysData.current_page = PAGE_HOME;
            printf("[KEY] PA3 Pressed: Switched to PAGE_HOME\r\n");
        }
        
        // ------------------ PA4 逻辑：切换到系统页 ------------------
        if (curr_k4 == 0 && last_k4 == 1) {
            g_SysData.current_page = PAGE_SYSTEM;
            printf("[KEY] PA4 Pressed: Switched to PAGE_SYSTEM\r\n");
        }

        // 3. 将当前状态保存，留作下一次 20ms 后进行对比
        last_k2 = curr_k2;
        last_k3 = curr_k3;
        last_k4 = curr_k4;
    }
}