#include "app_display.h"
#include "ssd1306_fonts.h"
#include "app_data.h" 
#include <stdio.h>

void Task_Display_Update(void)
{
    static uint32_t last_time = 0;
    char buf[32];
    
    // 100ms 刷新一次屏幕
    if (HAL_GetTick() - last_time >= 100)
    {
        last_time = HAL_GetTick();

        // 【极其重要】：由于页面会切换，必须先清空上一帧的显存！
        ssd1306_Fill(Black);        

// 核心状态机：根据当前状态，决定画什么内容
        switch (g_SysData.current_page)
        {
            case PAGE_HOME:
                // --- 渲染第一页：温湿度主页 ---
                if (g_SysData.is_sensor_error) {
                    ssd1306_SetCursor(10, 20);
                    ssd1306_WriteString("Sensor Error!", Font_11x18, White);
                } else {
                    sprintf(buf, "Temp: %d C", g_SysData.temperature);
                    ssd1306_SetCursor(10, 10);
                    ssd1306_WriteString(buf, Font_7x10, White);
                    
                    sprintf(buf, "Humi: %d %%", g_SysData.humidity);
                    ssd1306_SetCursor(10, 30);
                    ssd1306_WriteString(buf, Font_7x10, White);

                    sprintf(buf, "Light: %d %%", g_SysData.light_percent);
                    ssd1306_SetCursor(10, 50); // 放在屏幕最下方
                    ssd1306_WriteString(buf, Font_7x10, White);
                }
                break;
                
            case PAGE_SYSTEM:
                // --- 渲染第二页：系统状态页 ---
                ssd1306_SetCursor(5, 5);
                ssd1306_WriteString("- System Info -", Font_7x10, White);
                
                // 计算系统运行时间 (开机以来的秒数)
                uint32_t uptime_sec = HAL_GetTick() / 1000;
                sprintf(buf, "Uptime: %lu s", uptime_sec);
                ssd1306_SetCursor(5, 25);
                ssd1306_WriteString(buf, Font_7x10, White);
                
                // 读取 PC13 引脚的真实电平状态，判断灯是开是关
                // (注意：STM32 的 PC13 通常是低电平点亮)
                GPIO_PinState led_state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
                sprintf(buf, "LED: %s", (led_state == GPIO_PIN_RESET) ? "ON" : "OFF");
                ssd1306_SetCursor(5, 45);
                ssd1306_WriteString(buf, Font_7x10, White);
                break;
                
            default:
                break;
        }
        ssd1306_UpdateScreen();
    }
}