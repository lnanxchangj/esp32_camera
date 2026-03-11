# ESP32 Thermal Camera

**基于 ESP32 + Heimann HTPA 32×32 + OV3660 的红外热成像仪**

实时热成像与可见光融合显示系统，支持多种融合模式和伪彩色方案。

---

## 硬件规格


| 组件     | 型号                    | 参数                         |
| -------- | ----------------------- | ---------------------------- |
| MCU      | ESP32                   | 240MHz 双核, 2MB Flash       |
| PSRAM    | ESP-PSRAM64H            | 8MB, 80MHz Quad SPI          |
| 热传感器 | Heimann HTPA 32×32 5.0 | 32×32px, I2C 400kHz,\~8fps  |
| 摄像头   | OV3660                  | 3MP, QVGA RGB565 输出, 30fps |
| 显示屏   | ST7789 TFT              | 240×320, RGB565, SPI 80MHz  |

### 引脚分配

```
┌─────────────────────────────────────────────────┐
│                  ESP32 引脚分配                    │
├──────────────┬──────────┬───────────────────────┤
│ 功能          │ GPIO     │ 说明                  │
├──────────────┼──────────┼───────────────────────┤
│ HTPA I2C SDA │ GPIO21   │ 400kHz Fast Mode      │
│ HTPA I2C SCL │ GPIO22   │                       │
├──────────────┼──────────┼───────────────────────┤
│ CAM XCLK     │ GPIO0    │ 20MHz                 │
│ CAM SIOD     │ GPIO26   │ SCCB (I2C-like)       │
│ CAM SIOC     │ GPIO27   │                       │
│ CAM D0~D7    │ 4,5,18,19│ 8-bit DVP             │
│              │ 36,39,34 │                       │
│              │ 35       │                       │
│ CAM VSYNC    │ GPIO25   │                       │
│ CAM HREF     │ GPIO23   │                       │
│ CAM PCLK     │ GPIO22   │                       │
├──────────────┼──────────┼───────────────────────┤
│ LCD MOSI     │ GPIO13   │ SPI2, 80MHz, DMA      │
│ LCD SCLK     │ GPIO14   │                       │
│ LCD CS       │ GPIO15   │                       │
│ LCD DC       │ GPIO2    │                       │
│ LCD RST      │ GPIO12   │                       │
├──────────────┼──────────┼───────────────────────┤
│ BTN MODE     │ GPIO32   │ 模式切换 (内部上拉)    │
│ BTN PALETTE  │ GPIO33   │ 色板切换 (内部上拉)    │
└──────────────┴──────────┴───────────────────────┘
```

> **重要**: 以上引脚为参考配置，请根据你的 PCB 实际连线修改对应头文件中的宏定义。

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 Dual-Core Pipeline                  │
│                                                             │
│  ┌─── Core 0 ──────────────────┐  ┌─── Core 1 ───────────┐ │
│  │                              │  │                       │ │
│  │  ┌──────────┐  ┌──────────┐ │  │  ┌────────────────┐   │ │
│  │  │ HTPA32x32│  │  OV3660  │ │  │  │   Interpolate  │   │ │
│  │  │  ~8 fps  │  │  30 fps  │ │  │  │  32×32 → 240×  │   │ │
│  │  │  I2C     │  │  DVP     │ │  │  │  320 bilinear  │   │ │
│  │  └────┬─────┘  └────┬─────┘ │  │  └───────┬────────┘   │ │
│  │       │              │       │  │          │            │ │
│  │  ┌────▼──────────────▼─────┐ │  │  ┌───────▼────────┐   │ │
│  │  │  Double Buffers (PSRAM) │─┼──┼─▶│  Pseudocolor   │   │ │
│  │  │  thermal[2] + cam_fb   │ │  │  │  LUT Mapping   │   │ │
│  │  └─────────────────────────┘ │  │  └───────┬────────┘   │ │
│  │                              │  │          │            │ │
│  └──────────────────────────────┘  │  ┌───────▼────────┐   │ │
│                                    │  │  Alpha Blend   │   │ │
│                                    │  │  / PiP / Edge  │   │ │
│                                    │  └───────┬────────┘   │ │
│                                    │          │            │ │
│                                    │  ┌───────▼────────┐   │ │
│                                    │  │  OSD Overlay   │   │ │
│                                    │  │  Temp / Cross  │   │ │
│                                    │  └───────┬────────┘   │ │
│                                    │          │            │ │
│                                    │  ┌───────▼────────┐   │ │
│                                    │  │  ST7789 SPI    │   │ │
│                                    │  │  DMA Push      │   │ │
│                                    │  │  ~25-30 fps    │   │ │
│                                    │  └────────────────┘   │ │
│                                    └───────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 渲染管线耗时


| 阶段                        | 耗时       | 说明                       |
| --------------------------- | ---------- | -------------------------- |
| 双线性插值 32×32→240×320 | \~8ms      | PSRAM 访问受限             |
| 伪彩色 LUT 映射             | \~3ms      | 256 级查找表               |
| Alpha 混合                  | \~5ms      | 定点 8.8 整数运算          |
| OSD 叠加                    | \~1ms      | 十字准星 + 温度文字 + 色标 |
| SPI DMA 推送                | \~10ms     | 80MHz, 分块传输            |
| **总计**                    | **\~27ms** | **显示帧率 \~25-30fps**    |

---

## 功能特性

### 融合模式 (按键切换)


| 模式           | 说明                            |
| -------------- | ------------------------------- |
| `THERMAL_ONLY` | 纯热图显示                      |
| `CAMERA_ONLY`  | 纯摄像头画面                    |
| `ALPHA_BLEND`  | 可见光 + 热图 Alpha 混合 (默认) |
| `PIP`          | 画中画 (摄像头全屏 + 热图小窗)  |
| `EDGE_OVERLAY` | 摄像头 + 热图温度梯度边缘叠加   |

### 伪彩色方案


| 色板        | 风格                      |
| ----------- | ------------------------- |
| `IRONBOW`   | 铁弓色 (经典热成像, 默认) |
| `RAINBOW`   | 彩虹色                    |
| `WHITE_HOT` | 白热 (高温=白)            |
| `BLACK_HOT` | 黑热 (高温=黑)            |
| `LAVA`      | 熔岩色                    |

### 插值算法


| 算法     | 速度          | 质量   | 说明        |
| -------- | ------------- | ------ | ----------- |
| Nearest  | 最快 (\~3ms)  | 像素化 | 调试用      |
| Bilinear | 适中 (\~8ms)  | 平滑   | **推荐**    |
| Bicubic  | 较慢 (\~25ms) | 最佳   | 拍照/截图用 |

### OSD 信息

* 中心十字准星 + 中心点温度
* 最高温 / 最低温显示
* 右侧色标温度条

---

## 内存预算

### Flash (2MB)

```
0x000000 ┌──────────────┐
         │ Bootloader   │ 0x7000  (28 KB)
0x009000 ├──────────────┤
         │ NVS          │ 0x4000  (16 KB)
0x00D000 ├──────────────┤
         │ OTA Data     │ 0x2000  (8 KB)
0x00F000 ├──────────────┤
         │ PHY Init     │ 0x1000  (4 KB)
0x010000 ├──────────────┤
         │              │
         │  Application │ 0x1F0000 (1984 KB)
         │              │
         │  (编译优化-Os │
         │   禁 WiFi/BT │
         │   实际 ~1.2MB)│
         │              │
0x200000 └──────────────┘
```

### PSRAM (8MB)


| 缓冲区        | 大小         | 说明                    |
| ------------- | ------------ | ----------------------- |
| Camera FB ×2 | 300 KB       | QVGA RGB565 双缓冲      |
| interp\_temp  | 300 KB       | 240×320 float 温度矩阵 |
| thermal\_fb   | 150 KB       | 240×320 RGB565 热图    |
| fused\_fb     | 150 KB       | 240×320 RGB565 输出    |
| HTPA raw      | 2 KB         | 原始帧数据              |
| **总计**      | **\~900 KB** | **PSRAM 利用率 \~11%**  |

---

## 构建与烧录

### 环境要求

* ESP-IDF v5.1+ (推荐 v5.2)
* Python 3.8+
* CMake 3.16+

### 构建步骤

```bash
# 1. 配置 ESP-IDF 环境
. $IDF_PATH/export.sh

# 2. 设置目标芯片
idf.py set-target esp32

# 3. 配置 (可选, 修改引脚等)
idf.py menuconfig

# 4. 编译
idf.py build

# 5. 烧录
idf.py -p /dev/ttyUSB0 flash

# 6. 监控串口输出
idf.py -p /dev/ttyUSB0 monitor

# 编译+烧录+监控一键
idf.py -p /dev/ttyUSB0 flash monitor
```

### menuconfig 关键配置

进入 `idf.py menuconfig` 后确认以下选项:

```
Serial flasher config
  └─ Flash size: 2MB

Component config → ESP PSRAM
  └─ Support for external, SPI-connected RAM: [*]
  └─ SPI RAM config
       └─ Type: ESP-PSRAM64
       └─ Speed: 80MHz
       └─ Mode: Quad

Component config → ESP32-specific
  └─ CPU frequency: 240MHz

Compiler options
  └─ Optimization Level: -Os (size)
```

---

## 项目结构

```
esp32-thermal-cam/
├── CMakeLists.txt              # 顶层 CMake
├── sdkconfig.defaults          # 默认配置 (2MB Flash + PSRAM)
├── partitions.csv              # 自定义分区表
├── README.md                   # 本文档
│
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                  # 主程序 (双核流水线)
│   ├── app_button.h            # 按键处理头文件
│   └── app_button.c            # 按键处理 (防抖 + 回调)
│
└── components/
    ├── htpa32x32/              # Heimann HTPA 32×32 热传感器驱动
    │   ├── CMakeLists.txt
    │   ├── include/htpa32x32.h
    │   └── htpa32x32.c
    │
    ├── ov3660_capture/         # OV3660 摄像头采集
    │   ├── CMakeLists.txt
    │   ├── idf_component.yml   # esp32-camera 依赖
    │   ├── include/ov3660_capture.h
    │   └── ov3660_capture.c
    │
    ├── st7789_display/         # ST7789 SPI DMA 显示驱动
    │   ├── CMakeLists.txt
    │   ├── include/st7789_display.h
    │   └── st7789_display.c
    │
    └── thermal_fusion/         # 融合渲染引擎 (核心)
        ├── CMakeLists.txt
        ├── include/thermal_fusion.h
        └── thermal_fusion.c
```

---

## 调优指南

### 提升显示帧率

1. **使用最近邻插值** (`INTERP_NEAREST`): 省 \~5ms，适合动态场景
2. **减少 OSD**: 关闭色标和温度文字省 \~1ms
3. **纯热图模式**: 省去 Alpha 混合 \~5ms
4. **降低 SPI 传输量**: 仅更新变化区域 (脏矩形)

### 提升热图质量

1. **双三次插值** (`INTERP_BICUBIC`): 最佳画质，适合静态/拍照
2. **增大温度范围平滑系数**: `smooth = 0.95f` 减少闪烁
3. **校准发射率**: 根据被测物体材质设置 `emissivity`
4. **时域滤波**: 在 thermal\_task 中对连续帧做 IIR 低通滤波

### 2MB Flash 空间不够？

1. 确认已禁用 WiFi/BT (`CONFIG_WIFI_ENABLED=n`, `CONFIG_BT_ENABLED=n`)
2. 使用 `-Os` 优化 (`CONFIG_COMPILER_OPTIMIZATION_SIZE=y`)
3. 减少日志级别 (`CONFIG_LOG_DEFAULT_LEVEL_WARN=y`)
4. 去除未使用的 ESP-IDF 组件

---

## 已知限制

* HTPA32x32 帧率硬件限制 (\~4-8fps), 热图更新速度受限
* 2MB Flash 空间紧张, 无法同时启用 WiFi/BT
* OV3660 与 HTPA32x32 的 FOV (视场角) 不完全一致，融合画面可能有轻微偏移
* 字体为简易 5×7 像素字体，仅支持数字和基本符号

### FOV 校准建议

OV3660 视场角约 66°, HTPA32x32 视场角约 33°×33°。融合时需注意:

1. 将 OV3660 设为更窄的裁切窗口以匹配热传感器 FOV
2. 或在插值映射时添加偏移/缩放补偿参数
3. 具体校准值需要用标定板实测

---

## 扩展方向

* [ ]  WiFi 串流 (需 4MB+ Flash 或 OTA 分区调整)
* [ ]  SD 卡录制热图数据
* [ ]  温度报警 (蜂鸣器)
* [ ]  触摸屏选点测温
* [ ]  BLE 手机连接
* [ ]  时域降噪 (多帧平均)
* [ ]  自动 FOV 校准

---

## 许可证

MIT License

---

## 参考资料

* [Heimann HTPA 32×32 Application Note](https://www.heimannsensor.com/)
* [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
* [esp32-camera Driver](https://github.com/espressif/esp32-camera)
* [ST7789 Datasheet](https://www.newhavendisplay.com/)
