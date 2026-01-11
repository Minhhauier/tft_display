#ifndef CONTROL_LED
#define CONTROL_LED

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "led_strip.h"

#define COLOR_RED    0xFF0000
#define COLOR_GREEN  0x00FF00
#define COLOR_BLUE   0x0000FF
#define COLOR_ORANGE 0xFF8C00
#define COLOR_BLACK  0x000000
#define COLOR_WHITE  0xFFFFFF
#define COLOR_YELLOW 0xFFFF00
#define COLOR_PURPLE 0x800080
#define COLOR_CYAN   0x00FFFF
#define COLOR_PINK   0xFFC0CB
#define COLOR_PINK_LIGHT   0xFFB6C1
#define COLOR_AMBER        0xFFB347
#define COLOR_WHITE_WARM  0xFFF5E1


#define LED_GPIO    4
#define LED_NUMBER  4
#define LED_LEN     1
/**
 * @brief Struct to store a 24-bit RGB color
 */



typedef struct {
    uint8_t r;  // Red component (0-255)
    uint8_t g;  // Green component (0-255)
    uint8_t b;  // Blue component (0-255)
} rgb_t;

typedef enum {
    mode_1 = 0,
    mode_2 = 1,
    mode_3 = 2,
    mode_4 = 3,
    mode_5 = 4
} mode;
 extern mode mode_rgb;
typedef struct {
    led_strip_handle_t handle;
    int led_gpio;
    int led_count;
    int group_size;
} led_config_t;

typedef struct 
{
    led_config_t *led_strip;
    int color;
    int index;
}led_task_param_t;


void start_charge_led();
typedef enum {
    LED_OFF = 0,
    LED_READY = 1,
    LED_CHARGING = 2,
    LED_WARNING = 3,
    LED_ERROR = 4,
    LED_WAITTING =5
} led_status_t;

extern bool check_led_init;
extern led_config_t charge_led;
extern bool active;
extern int gate;
extern led_task_param_t *params;


void charging_led_by_status(int gate_num, int status);
void all_led_by_status(int status);
void led_blink(led_task_param_t *param);
void led_strip_init(led_config_t *led_strip);
void led_strip_clear_all(led_config_t *led_strip);
// Convert 0xRRGGBB to rgb_t; inline so all TUs can use it
static inline rgb_t rgb_from_hex(uint32_t hex_code)
{
    rgb_t color;
    color.r = (hex_code >> 16) & 0xFF;
    color.g = (hex_code >> 8) & 0xFF;
    color.b = hex_code & 0xFF;
    return color;
}
void set_single_led(led_config_t *led_strip, int color, int index);
void set_group_led(led_config_t *led_strip, int color, int index);
void set_all_led(led_config_t *led_strip, int color);
void clear_group_led(led_config_t *led_strip,int index);
void set_mode_rgb(void *pvParameters);

#endif