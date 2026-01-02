idf_component_register(
    SRCS "main_lvgl.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd driver esp_lcd_st7796
)
