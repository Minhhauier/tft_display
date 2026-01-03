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
#include "string.h"
#include "stdio.h"

#include "parameter.h"
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
static lv_obj_t *body, *header, *footer;
static lv_obj_t *txt_time, *txt_header, *txt_signal;
static object_parameter_t gate_obj[10];
char data_str[50];

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

void display_init(void){
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
}
void set_background(lv_obj_t* label, lv_color_t color_bg,int hight, int width, lv_align_t pos, int x_ofs, int y_ofs){
    lv_obj_set_style_bg_color(label,color_bg, 0);
    lv_obj_set_size(label, width, hight);
    lv_obj_align(label, pos, x_ofs, y_ofs);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0); // Độ đậm đặc (ko trong suốt)
    //options 
    //lv_obj_set_style_text_align(label,LV_ALIGN_CENTER, 0);
    //lv_obj_set_style_radius(label, 0, 0); // Vuông góc
    //lv_obj_set_style_border_width(label, 0, 0); // Không viền
}
void set_text(lv_obj_t* label, const char* text, lv_align_t pos, lv_color_t color_text, int x_ofs, int y_ofs){
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color_text, 0);
    lv_obj_align(label, pos, x_ofs, y_ofs);
}

void ui_layout_example(void)
{
    // Xóa sạch màn hình trước khi vẽ
    lv_obj_clean(lv_scr_act());

    // ==========================================
    // PHẦN 1: HEADER (Thanh tiêu đề màu xanh)
    // ==========================================
    lv_obj_t * header = lv_obj_create(lv_scr_act()); // Tạo container con của màn hình
    
    // 1. Kích thước & Vị trí
    lv_obj_set_size(header, 480, 50);  // Rộng 480 (full), Cao 50
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0); // Căn lên đỉnh
    
    // 2. Màu sắc (Quan trọng)
    lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_BLUE), 0); // Màu xanh
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0); // Độ đậm đặc (ko trong suốt)
    
    // 3. Xóa viền và bo góc (Mặc định LVGL có viền và bo tròn, nhìn ko giống thanh bar)
    lv_obj_set_style_radius(header, 0, 0); // Vuông góc
    lv_obj_set_style_border_width(header, 0, 0); // Không viền

    // 4. Thêm chữ VÀO TRONG header (Lưu ý tham số đầu tiên là 'header')
    lv_obj_t * txt_header = lv_label_create(header); 
    lv_label_set_text(txt_header, "HE THONG GIAM SAT");
    lv_obj_set_style_text_color(txt_header, lv_color_white(), 0);
    lv_obj_center(txt_header); // Căn giữa cái hộp header


    // ==========================================
    // PHẦN 2: BODY (Khu vực chính màu xám)
    // ==========================================
    lv_obj_t * body = lv_obj_create(lv_scr_act());
    
    // Rộng 480, Cao = 320 - 50 (header) - 40 (footer) = 230
    lv_obj_set_size(body, 480, 230); 
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 50); // Cách đỉnh 50px (ngay dưới header)
    
    // Màu nền
    lv_obj_set_style_bg_color(body, lv_palette_lighten(LV_PALETTE_GREY, 4), 0); // Xám rất nhạt
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_style_border_width(body, 0, 0);

    // Thêm nội dung vào Body
    lv_obj_t * txt_body = lv_label_create(body);
    lv_label_set_text(txt_body, "Nhiet do: 25*C\nDo am: 60%");
    lv_obj_set_style_text_color(txt_body, lv_color_black(), 0);
    lv_obj_align(txt_body, LV_ALIGN_CENTER, 0, 0);


    // ==========================================
    // PHẦN 3: FOOTER (Thanh trạng thái màu cam)
    // ==========================================
    lv_obj_t * footer = lv_obj_create(lv_scr_act());
    
    lv_obj_set_size(footer, 480, 40); // Cao 40
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0); // Căn đáy màn hình
    
    lv_obj_set_style_bg_color(footer, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_border_width(footer, 0, 0);

    // Thêm chữ vào Footer
    lv_obj_t * txt_footer = lv_label_create(footer);
    lv_label_set_text(txt_footer, "WiFi: Connected | IP: 192.168.1.100");
    lv_obj_center(txt_footer);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Khoi dong (Fix Double Image Mode)...");

    // --- KHU VỰC SỬA LỖI HIỂN THỊ ---
    display_init();
    // lv_obj_clean(lv_scr_act());
    // lv_obj_t* scr = lv_scr_act();
    // set_text(scr, "Hello World", LV_ALIGN_CENTER, lv_color_make(255, 0, 0), lv_color_make(200, 200, 200));
    // // Test hiển thị
 //   ui_layout_example();
//==========================================
    // Tạo bố cục chính
    header = lv_obj_create(lv_scr_act());
    set_background(header, lv_palette_main(LV_PALETTE_BLUE), 50, 480, LV_ALIGN_TOP_MID, 0, 0);
    txt_time = lv_label_create(header);
    txt_header = lv_label_create(header);
    txt_signal = lv_label_create(header);
    set_text(txt_time, "Time 00:12", LV_ALIGN_RIGHT_MID, lv_color_make(0, 0, 0),0,0);
    set_text(txt_header, "EVSAFE", LV_ALIGN_CENTER, lv_color_white(),0,0);
    set_text(txt_signal, LV_SYMBOL_WIFI, LV_ALIGN_LEFT_MID, lv_color_make(0, 0, 0),0,0);
    body = lv_obj_create(lv_scr_act());
    set_background(body, lv_palette_lighten(LV_PALETTE_GREY, 4), 230, 480, LV_ALIGN_TOP_MID, 0, 50);
    // gate_obj[0].ob_name = lv_label_create(body);
    // gate_obj[0].ob_value = lv_label_create(body);
    // set_text(gate_obj[0].ob_name, "Gate 1: ", LV_ALIGN_TOP_LEFT, lv_color_black(),0,10);
    // set_text(gate_obj[0].ob_value, "CLOSED", LV_ALIGN_TOP_LEFT, lv_color_make(0, 150, 0),70,10);    
    for(int i=0; i<5; i++){
        gate_obj[i].ob_name = lv_label_create(body);
        gate_obj[i].ob_value = lv_label_create(body);
        snprintf(data_str, sizeof(data_str), "Gate %d: ", i+1);
        set_text(gate_obj[i].ob_name, data_str, LV_ALIGN_TOP_LEFT, lv_color_black(),0,10 + i*40);
        set_text(gate_obj[i].ob_value,LV_SYMBOL_CLOSE , LV_ALIGN_TOP_LEFT, lv_color_make(0, 150, 0),70,10 + i*40);    
    }
    for(int i=5; i<10; i++){
        gate_obj[i].ob_name = lv_label_create(body);
        gate_obj[i].ob_value = lv_label_create(body);
        snprintf(data_str, sizeof(data_str), "Gate %d: ", i+1);
        set_text(gate_obj[i].ob_name, data_str, LV_ALIGN_TOP_RIGHT, lv_color_black(),-80,10 + (i-5)*40);
        set_text(gate_obj[i].ob_value, LV_SYMBOL_CLOSE, LV_ALIGN_TOP_RIGHT, lv_color_make(0, 150, 0),0,10 + (i-5)*40);    
    }
    footer = lv_obj_create(lv_scr_act());
    set_background(footer, lv_palette_main(LV_PALETTE_ORANGE), 40, 480, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Vòng lặp chính
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}