#include "camera.h"
#include "lcd.h"

#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_heap_caps.h"

static const char *TAG = "camera";

/* ------------------------------------------------------------------ */
/*  90° CCW 转置：320×240 → 240×320                                   */
/*  公式：dst[y][x] = src[x][W-1-y]                                   */
/*  dst: 240×320  src: 320×240                                        */
/* ------------------------------------------------------------------ */
static void rotate_ccw_90(const uint16_t *src, uint16_t *dst)
{
    /* src 宽=320 高=240，dst 宽=240 高=320 */
    for (int row = 0; row < CAM_FRAME_HEIGHT; row++) {       /* 0..239 */
        for (int col = 0; col < CAM_FRAME_WIDTH; col++) {    /* 0..319 */
            /* src 像素 (row, col) → dst 像素 (W-1-col, row) */
            dst[(CAM_FRAME_WIDTH - 1 - col) * CAM_FRAME_HEIGHT + row]
                = src[row * CAM_FRAME_WIDTH + col];
        }
    }
}

/* ------------------------------------------------------------------ */
/*  初始化                                                              */
/* ------------------------------------------------------------------ */
esp_err_t camera_init(void)
{
    /* 1. 使能 I2C 外部上拉 */
    gpio_config_t pull_cfg = {
        .pin_bit_mask = BIT64(I2C_PULLUP_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pull_cfg));
    gpio_set_level(I2C_PULLUP_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 2. 手动复位 OV3660 */
    gpio_set_direction(RESET_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(RESET_GPIO_NUM, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(RESET_GPIO_NUM, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    /* 3. 摄像头配置 */
    camera_config_t config = {
        .pin_pwdn     = PWDN_GPIO_NUM,
        .pin_reset    = RESET_GPIO_NUM,
        .pin_xclk     = XCLK_GPIO_NUM,
        .pin_sccb_sda = SIOD_GPIO_NUM,
        .pin_sccb_scl = SIOC_GPIO_NUM,
        .pin_vsync    = VSYNC_GPIO_NUM,
        .pin_href     = HREF_GPIO_NUM,
        .pin_pclk     = PCLK_GPIO_NUM,
        .pin_d0 = D0_GPIO_NUM, .pin_d1 = D1_GPIO_NUM,
        .pin_d2 = D2_GPIO_NUM, .pin_d3 = D3_GPIO_NUM,
        .pin_d4 = D4_GPIO_NUM, .pin_d5 = D5_GPIO_NUM,
        .pin_d6 = D6_GPIO_NUM, .pin_d7 = D7_GPIO_NUM,

        .xclk_freq_hz  = 10000000,
        .pixel_format  = PIXFORMAT_RGB565,
        .frame_size    = CAM_FRAMESIZE,        /* QVGA 320×240 */
        .fb_count      = 2,
        .fb_location   = CAMERA_FB_IN_PSRAM,
        .grab_mode     = CAMERA_GRAB_LATEST,
        .jpeg_quality  = 12,
        .sccb_i2c_port = 1,
    };

    esp_err_t ret = esp_camera_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init 失败: 0x%x", ret);
        return ret;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_vflip(sensor, 1);
        sensor->set_hmirror(sensor, 0);
        sensor->set_brightness(sensor, 1);
        sensor->set_saturation(sensor, 0);
    }

    ESP_LOGI(TAG, "OV3660 初始化完成 QVGA 320×240 RGB565");
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  摄像头 → LCD 显示任务                                              */
/* ------------------------------------------------------------------ */
void camera_lcd_task(void *arg)
{
    esp_lcd_panel_handle_t panel = lcd_get_panel_handle();
    if (!panel) {
        ESP_LOGE(TAG, "LCD 句柄无效，任务退出");
        vTaskDelete(NULL);
        return;
    }

    /* 分配转置缓冲（240×320 × 2字节 = 153600 字节，放 PSRAM） */
    const size_t rot_buf_size = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
    uint16_t *rot_buf = heap_caps_malloc(rot_buf_size, MALLOC_CAP_SPIRAM);
    if (!rot_buf) {
        ESP_LOGE(TAG, "转置缓冲分配失败（需要 %u 字节 PSRAM）", (unsigned)rot_buf_size);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "转置缓冲分配成功，开始推流");

    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (fb->format == PIXFORMAT_RGB565
            && fb->len >= CAM_FRAME_WIDTH * CAM_FRAME_HEIGHT * 2)
        {
            /* 320×240 → 旋转 → 240×320，填满整个屏幕 */
            rotate_ccw_90((const uint16_t *)fb->buf, rot_buf);

            esp_lcd_panel_draw_bitmap(
                panel,
                0, 0,
                LCD_H_RES, LCD_V_RES,   /* 240, 320 */
                rot_buf
            );
        }

        esp_camera_fb_return(fb);
        taskYIELD();
    }
}