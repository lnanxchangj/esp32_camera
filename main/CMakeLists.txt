idf_component_register(
    SRCS
        "main.c"
        "lcd/lcd.c"
        "camera/camera.c"
    INCLUDE_DIRS
        "."
        "lcd"
        "camera"
    # REQUIRES
    #     esp_psram
)