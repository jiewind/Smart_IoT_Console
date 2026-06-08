#include "app_sensor.h"
#include "dht11.h"
#include "app_data.h" // 引入数据中枢
extern ADC_HandleTypeDef hadc1; // 声明外部的 ADC 句柄

// 传感器任务调度，放入 main 里的 while(1) 中
void Task_Sensor_Update(void)
{
    static uint32_t last_time = 0;
    static uint32_t last_light_time = 0;
    
    static uint8_t dht_error_count = 0;
    // 1000ms 采集一次
    if (HAL_GetTick() - last_time >= 1000) 
    {
        last_time = HAL_GetTick();
        
        // 采集数据，直接存入全局黑板中
        if (DHT11_Read_Data(&g_SysData.temperature, &g_SysData.humidity) == 0) {
            dht_error_count = 0;
            g_SysData.is_sensor_error = 0; // 成功
        } else {
            // 读取失败时，不立刻报错，而是累加错误次数
            dht_error_count++;
            // 连续失败 3 次（即 3 秒都读错），才真正把错误标志拉高显示在屏幕上
            if (dht_error_count >= 3) {
                g_SysData.is_sensor_error = 1;
            }
        }
    }

    // 【新增】：ADC 光照度采集逻辑 (500ms 轮询)
    if (HAL_GetTick() - last_light_time >= 500)
    {
        last_light_time = HAL_GetTick();
        
        // 1. 启动 ADC 转换
        HAL_ADC_Start(&hadc1);
        
        // 2. 等待转换完成 (超时时间设为 1ms，极快，不会阻塞系统)
        if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK)
        {
            // 3. 读取 12 位 ADC 值 (范围是 0 ~ 4095)
            uint16_t adc_value = HAL_ADC_GetValue(&hadc1);
            
            // 4. 将 0~4095 的原始值，映射成 0~100 的百分比
            // 注意：通常光线越暗，AO 电压越高，ADC 值越大。我们可以做一个反向映射：
            // 光照度百分比 = 100 - (adc_value * 100 / 4095)
            g_SysData.light_percent = 100 - (adc_value * 100 / 4095);
        }
    }
}