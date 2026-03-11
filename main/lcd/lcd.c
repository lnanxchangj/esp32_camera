#include "lcd.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"

static const char *TAG = "LCD_DRIVER";

/* 保存全局 LCD 句柄 */
static esp_lcd_panel_handle_t panel_handle = NULL;

esp_err_t lcd_init(void)
{
    ESP_LOGI(TAG, "Initialize SPI bus");
    // 定义 DMA 一次最多传输的字节数 (这里设定为每次刷 40 行的显存大小)
    size_t max_transfer_size = LCD_H_RES * 40 * sizeof(uint16_t);
    spi_bus_config_t buscfg = ST7789_PANEL_BUS_SPI_CONFIG(LCD_PIN_SCLK, LCD_PIN_MOSI, max_transfer_size);

    // ESP32 标准版推荐使用 SPI2_HOST (HSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = ST7789_PANEL_IO_SPI_CONFIG(LCD_PIN_CS, LCD_PIN_DC, NULL, NULL);
    // 绑定 IO 到对应的 SPI 主机
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install ST7789 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB, // 如果显示颜色颠倒，可尝试改为 LCD_RGB_ENDIAN_BGR
        .bits_per_pixel = 16,
    };
    // 调用你提供的组件 API 创建 Panel
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    // 复位并初始化 Panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // ST7789 屏幕大多默认硬件反色，所以这里打开反色 (视具体屏幕而定，若颜色不对可改为 false)
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));

    // 初始化背光引脚
    if (LCD_PIN_BLK >= 0)
    {
        ESP_LOGI(TAG, "Turn on backlight");
        gpio_config_t bk_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << LCD_PIN_BLK};
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
        lcd_set_backlight(true);
    }

    // 开启屏幕显示
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "LCD initialization successful");
    return ESP_OK;
}

void lcd_set_backlight(bool on)
{
    if (LCD_PIN_BLK >= 0)
    {
        gpio_set_level(LCD_PIN_BLK, on ? 1 : 0);
    }
}

void lcd_clear(uint16_t color)
{
    if (!panel_handle)
        return;

    // 分块刷屏：每次刷新 40 行，避免一次分配 240x320x2=150KB 内存导致失败
    const int lines_per_chunk = 40;
    size_t buffer_pixels = LCD_H_RES * lines_per_chunk;

    // 使用 DMA 专用内存
    uint16_t *buffer = (uint16_t *)heap_caps_malloc(buffer_pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buffer)
    {
        ESP_LOGE(TAG, "No memory for clear buffer");
        return;
    }

    // SPI 传输需处理大小端问题：将 RGB565 (Little Endian) 转为 SPI 识别的 Big Endian
    uint16_t swapped_color = (color << 8) | (color >> 8);
    for (size_t i = 0; i < buffer_pixels; i++)
    {
        buffer[i] = swapped_color;
    }

    // 分段发送到屏幕
    for (int y = 0; y < LCD_V_RES; y += lines_per_chunk)
    {
        int y_end = y + lines_per_chunk;
        if (y_end > LCD_V_RES)
        {
            y_end = LCD_V_RES;
        }
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y_end, buffer);
    }

    free(buffer);
}

esp_lcd_panel_handle_t lcd_get_panel_handle(void)
{
    return panel_handle;
}