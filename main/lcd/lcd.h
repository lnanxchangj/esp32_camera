#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "hal/spi_ll.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_st7789.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define LCD_PIN_MOSI 12
#define LCD_PIN_SCLK 13
#define LCD_PIN_CS 15
#define LCD_PIN_DC 2
#define LCD_PIN_RST -1 /* 无 RST 引脚 */
#define LCD_PIN_BLK 4

#define LCD_H_RES 240
#define LCD_V_RES 320

    /**
     * @brief 初始化 LCD (包括 SPI 总线、Panel IO、驱动并点亮背光)
     * @return esp_err_t ESP_OK 表示成功
     */
    esp_err_t lcd_init(void);

    /**
     * @brief 使用指定颜色清屏 (RGB565格式)
     * @param color 16位颜色值
     */
    void lcd_clear(uint16_t color);

    /**
     * @brief 设置背光开关
     * @param on true 打开, false 关闭
     */
    void lcd_set_backlight(bool on);

    /**
     * @brief 获取屏幕句柄，以便外部调用 esp_lcd 框架的高级绘图 API（如 LVGL 对接）
     */
    esp_lcd_panel_handle_t lcd_get_panel_handle(void);

#ifdef __cplusplus
}
#endif