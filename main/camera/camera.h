#pragma once

#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ────────── 引脚定义 ────────── */
#define PWDN_GPIO_NUM   17
#define RESET_GPIO_NUM  26
#define XCLK_GPIO_NUM   14   /* ⚠️ 与 LCD_SCLK(13) 不冲突，但注意与 LCD 分属不同外设 */
#define PCLK_GPIO_NUM   22
#define SIOD_GPIO_NUM   23
#define SIOC_GPIO_NUM   18
#define VSYNC_GPIO_NUM  21
#define HREF_GPIO_NUM   27
#define D0_GPIO_NUM     34
#define D1_GPIO_NUM     33
#define D2_GPIO_NUM     25
#define D3_GPIO_NUM     35
#define D4_GPIO_NUM     39
#define D5_GPIO_NUM     38
#define D6_GPIO_NUM     37
#define D7_GPIO_NUM     36
#define I2C_PULLUP_GPIO 19   /* 高电平 = 使能外部上拉 */

/* ────────── 采集分辨率 ────────── */
/* 240×240：竖屏无需旋转，PSRAM 每帧 = 240×240×2 = 115200 Bytes */
#define CAM_FRAME_WIDTH   320
#define CAM_FRAME_HEIGHT  240
#define CAM_FRAMESIZE     FRAMESIZE_QVGA

/**
 * @brief 初始化 OV3660 摄像头
 * @return ESP_OK / 错误码
 */
esp_err_t camera_init(void);

/**
 * @brief 摄像头 → LCD 显示任务（FreeRTOS Task 入口）
 *        建议栈大小 4096，优先级 5
 */
void camera_lcd_task(void *arg);

#ifdef __cplusplus
}
#endif