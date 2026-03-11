#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lcd.h"
#include "camera.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "系统启动");

    /* 1. 初始化 LCD */
    if (lcd_init() != ESP_OK) {
        ESP_LOGE(TAG, "LCD 初始化失败，停止");
        return;
    }
    ESP_LOGI(TAG, "LCD 初始化完成");

    /* 2. 黑色清屏（留上下黑边） */
    lcd_clear(0x0000);

    /* 3. 初始化摄像头 */
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败，停止");
        return;
    }
    ESP_LOGI(TAG, "摄像头初始化完成");

    /* 4. 启动摄像头 → LCD 显示任务
     *    栈: 4096 words | 优先级: 5 | 绑定核心 0（核心 1 留给 WiFi/BT） */
    xTaskCreatePinnedToCore(
        camera_lcd_task,
        "cam_lcd",
        4096,
        NULL,
        5,
        NULL,
        0
    );

    /* app_main 不退出，保持 idle */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}