#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lcd.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "System start.");

    if (lcd_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "LCD init failed!");
        return;
    }
    ESP_LOGI(TAG, "LCD init successful!");

    while (1)
    {
        ESP_LOGI(TAG, "Fill Screen: RED");
        lcd_clear(0xF800);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}