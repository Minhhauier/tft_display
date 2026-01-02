#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_lcd_st7796.h"

#define LCD_HOST SPI2_HOST

//=== Cấu hình chân SPI (Giữ nguyên) ===//
#define PIN_NUM_MOSI 23
#define PIN_NUM_SCLK 18
#define PIN_NUM_CS   15
#define PIN_NUM_DC   2
#define PIN_NUM_RST  27
#define PIN_NUM_BCKL 21

//=== Cấu hình màn hình ===// 
#define LCD_H_RES            480
#define LCD_V_RES            320

// QUAN TRỌNG: Giảm tốc độ xuống 10MHz để tránh nhiễu tín hiệu gây nhân đôi ảnh
#define LCD_PIXEL_CLOCK_HZ   (10 * 1000 * 1000) 

static const char *TAG = "LCD_FIX_DOUBLE";
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_drv_t * global_disp_drv = NULL;

static void init_backlight(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_NUM_BCKL,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
    gpio_set_level(PIN_NUM_BCKL, 1);
}

// Callback báo DMA xong (Giữ nguyên từ lần trước vì cái này đúng)
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    if (global_disp_drv) {
        lv_disp_flush_ready(global_disp_drv);
    }
    return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    if (panel_handle == NULL) return;
    global_disp_drv = drv;
    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2 + 1;
    int y2 = area->y2 + 1;
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, (uint16_t *)color_map);
}

static inline uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Khoi dong (Fix Double Image Mode)...");
    init_backlight();

    // 1. SPI Config
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 2. IO Config
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ, // Đã giảm xuống 10MHz
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // 3. Panel Config
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // --- KHU VỰC SỬA LỖI HIỂN THỊ ---
    
    // Reset toạ độ offset về 0 (quan trọng cho ST7796)
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));

    // Cấu hình xoay ngang: 
    // Swap XY = true (đổi trục)
    // Mirror = false (không lật gương)
    // Nếu chữ bị ngược, hãy thử đổi Mirror(true, false) hoặc (false, true)
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false)); 
    
    // Nếu màu bị sai (trắng thành đen), uncomment dòng dưới:
    // ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // 4. LVGL Init
    lv_init();
    static lv_disp_draw_buf_t draw_buf;
    static uint16_t buf[LCD_H_RES * 40]; 
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LCD_H_RES * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Test hiển thị
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(0,0,0), 0);
    
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello World");
    lv_obj_set_style_text_color(label, lv_color_make(0, 255, 0), 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    // Vòng lặp chính
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}