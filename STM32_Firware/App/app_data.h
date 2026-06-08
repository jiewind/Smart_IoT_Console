#ifndef __APP_DATA_H
#define __APP_DATA_H

#include <stdint.h>

// 1. 定义系统的 UI 页面枚举（状态集合）
typedef enum {
    PAGE_HOME = 0,   // 主页（温湿度）
    PAGE_SYSTEM      // 系统页（运行时间与状态）
} PageState_e;

// 2. 升级系统全局数据的结构体
typedef struct {
    uint8_t temperature;
    uint8_t humidity;
    uint8_t is_sensor_error;
    
    uint8_t light_percent;
    PageState_e current_page;  // 记录当前屏幕处于哪一页
    
} SystemData_t;

extern SystemData_t g_SysData;

#endif