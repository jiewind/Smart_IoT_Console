#include "app_led.h"
#include "app_data.h" // 获取光照度数据
#include <stdio.h>

void Task_Auto_Light(void)
{
    static uint32_t last_time = 0;
    
    // 100ms 检查一次光照状态足够了
    if (HAL_GetTick() - last_time >= 100)
    {
        last_time = HAL_GetTick();
        
        // 核心内功：迟滞比较算法 (Hysteresis)
        if (g_SysData.light_percent <= 30) 
        {
            // 极度昏暗，开启外接 LED (PB0 输出高电平 3.3V)
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        }
        else if (g_SysData.light_percent >= 50) 
        {
            // 光线充足，关闭外接 LED (PB0 输出低电平 0V)
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        }
        // 注意：如果光照在 31% ~ 49% 之间，代码什么都不做，维持上一次的状态！
    }
}