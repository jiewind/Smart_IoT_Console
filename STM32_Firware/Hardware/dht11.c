#include "dht11.h"

// 极其精准的微秒级延时函数 (利用 ARM Cortex-M 内核 SysTick)
void DHT11_Delay_us(uint16_t us)
{
    // 计算需要消耗多少个 CPU 时钟周期
    uint32_t ticks = (HAL_RCC_GetHCLKFreq() / 1000000) * us; 
    uint32_t start_val = SysTick->VAL; // 记录当前的滴答定时器数值
    
    while (1) 
    {
        uint32_t current_val = SysTick->VAL;
        uint32_t elapsed;
        
        // SysTick 是一个倒计数器，需要处理溢出翻转的情况
        if (start_val >= current_val) {
            elapsed = start_val - current_val;
        } else {
            elapsed = start_val + (SysTick->LOAD - current_val);
        }
        
        if (elapsed >= ticks) {
            break; // 精确到达指定微秒，立刻跳出！
        }
    }
}

// 把 PA0 设置为输出模式 (单片机喊话)
void DHT11_Set_Pin_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// 把 PA0 设置为输入模式 (单片机听话)
void DHT11_Set_Pin_In(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// 读取一次温湿度数据
// 返回值: 0成功，1失败
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi)
{
    uint8_t buf[5];
    uint8_t i, j;
    
    // 1. 主机发送起始信号
    DHT11_Set_Pin_Out();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_Delay(20); // 拉低至少 18ms
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
    DHT11_Delay_us(30); // 拉高 20~40us 等待 DHT11 响应
    
    // 2. 主机切换为输入，等待 DHT11 回应
    DHT11_Set_Pin_In();
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) 
    {
        while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET); // 等待DHT11拉低结束
        while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET);   // 等待DHT11拉高结束
        
        // 3. 开始接收 40 bit (5字节) 的数据
        for (i = 0; i < 5; i++) 
        {
            buf[i] = 0;
            for (j = 0; j < 8; j++) 
            {
                while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET); // 等待低电平结束
                DHT11_Delay_us(40); // 延时 40us，判断高电平还在不在
                
                if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) 
                {
                    buf[i] |= (1 << (7 - j)); // 写 1
                    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET); // 等待高电平结束
                }
            }
        }
        
        // 4. 校验数据
        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4]) 
        {
            *humi = buf[0];
            *temp = buf[2];
            return 0; // 成功
        }
    }
    return 1; // 失败
}