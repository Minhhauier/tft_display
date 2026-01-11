#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

#include "control_led_rgb.h"

int color[10] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_ORANGE, COLOR_PINK_LIGHT, COLOR_WHITE, COLOR_YELLOW, COLOR_PURPLE, COLOR_CYAN, COLOR_PINK};
mode mode_rgb = mode_1;
led_config_t charge_led = {
    .handle = NULL,
    .led_gpio = LED_GPIO,
    .led_count = LED_NUMBER,
    .group_size = LED_LEN,
};
led_task_param_t *params;
void start_charge_led()
{
    led_strip_init(&charge_led);
    set_group_led(&charge_led, COLOR_RED, 3);
    printf("Start charge LED\n");
}

bool check_led_init = false;
bool active = false;
int gate;

void all_led_by_status(int status)
{
    if (!check_led_init)
    {
        led_strip_init(&charge_led);
        check_led_init = true;
    }
    switch (status)
    {
    case LED_OFF:
        led_strip_clear_all(&charge_led);
        break;
    case LED_READY:
        set_all_led(&charge_led, COLOR_BLUE);
        break;
    case LED_CHARGING:
        set_all_led(&charge_led, COLOR_GREEN);
        break;
    case LED_WARNING:
        set_all_led(&charge_led, COLOR_ORANGE);
        break;
    case LED_ERROR:
        set_all_led(&charge_led, COLOR_RED);
        break;
    default:
        printf("Invalid LED status\n");
        break;
    }
    // set_group_led(&charge_led,COLOR_RED,7);
}
static inline int rand_in_range(int min, int max) {
    uint32_t r = esp_random();              // 32-bit random
    return min + (r % (max - min + 1));     // ví dụ 1..10
}

// Scale a hex color by brightness level (0-255)
static void set_all_led_dimmed(led_config_t *led_strip, int color, uint8_t level)
{
    rgb_t base = rgb_from_hex(color);
    rgb_t scaled = {
        .r = (base.r * level) / 255,
        .g = (base.g * level) / 255,
        .b = (base.b * level) / 255,
    };

    for (int i = 0; i < led_strip->led_count; i++) {
        led_strip_set_pixel(led_strip->handle, i, scaled.r, scaled.g, scaled.b);
    }
    led_strip_refresh(led_strip->handle);
}

// Hiệu ứng thở với hai đầu LED
static void render_two_head_fade(led_config_t *led_strip, int color, int head_l, int head_r, int tail_decay)
{
    rgb_t base = rgb_from_hex(color);
    for (int i = 0; i < led_strip->led_count; i++) {
        int dist_l = (head_l >= 0) ? abs(i - head_l) : 1000;
        int dist_r = (head_r >= 0 && head_r < led_strip->led_count) ? abs(i - head_r) : 1000;
        int dist = (dist_l < dist_r) ? dist_l : dist_r;

        int level = 255 - dist * tail_decay;  // linear fade per LED distance
        if (level < 0) level = 0;

        uint8_t r = (base.r * level) / 255;
        uint8_t g = (base.g * level) / 255;
        uint8_t b = (base.b * level) / 255;
        led_strip_set_pixel(led_strip->handle, i, r, g, b);
    }
    led_strip_refresh(led_strip->handle);
}

// sáng tắt tất cả LED với hiệu ứng thở
void fade_all_breathe(int color, int steps, int delay_ms)
{
    if (!check_led_init) {
        led_strip_init(&charge_led);
        check_led_init = true;
    }

    // Guard against invalid step count
    int step = (steps <= 0) ? 16 : 255 / steps;
    if (step <= 0) step = 1;

    // Fade in
    for (int lvl = 0; lvl <= 255; lvl += step) {
        set_all_led_dimmed(&charge_led, color, (uint8_t)lvl);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    // Ensure full brightness
    set_all_led_dimmed(&charge_led, color, 255);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    // Fade out
    for (int lvl = 255; lvl >= 0; lvl -= step) {
        set_all_led_dimmed(&charge_led, color, (uint8_t)lvl);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}
void set_mode_rgb(void *pvParameters)
{
    if (!check_led_init)
    {
        led_strip_init(&charge_led);
        check_led_init = true;
    }

    const int delay_ms = 120;
    while (1)
    {
        if (mode_rgb != mode_1 && mode_rgb != mode_2 && mode_rgb != mode_3 && mode_rgb != mode_4&& mode_rgb != mode_5)
        {
            break;
        }
        if (mode_rgb == mode_1)
        {
            for (int i = 0; i < 4; i++)
            {
                // Turn everything off, then light two consecutive LEDs to create a chase effect
                led_strip_clear_all(&charge_led);

                int first = i % charge_led.led_count;
                int second = (i + 1) % charge_led.led_count;

                // Random colors for the two chasing LEDs
                uint32_t color_first = color[rand_in_range(0, 9)];
                uint32_t color_second = color[rand_in_range(0, 9)];

                set_group_led(&charge_led, color_first, first + 1);
                set_group_led(&charge_led, color_second, second + 1);

                vTaskDelay(pdMS_TO_TICKS(delay_ms));
            }

           
           /* code */
        }
         if (mode_rgb == mode_2){
            for (int i = 0; i < 4; i++)
            {
                int first = i % charge_led.led_count;
                int second = (i + 1) % charge_led.led_count;

                // Random colors for the two chasing LEDs
                uint32_t color_first = color[rand_in_range(0, 9)];
                uint32_t color_second = color[rand_in_range(0, 9)];

                set_group_led(&charge_led, color_first, first + 1);
                set_group_led(&charge_led, color_second, second + 1);

                vTaskDelay(pdMS_TO_TICKS(2000));
            }
 
         }
         if (mode_rgb == mode_3){
            set_all_led(&charge_led, COLOR_ORANGE);
            vTaskDelay(pdMS_TO_TICKS(2000));
         }
        if (mode_rgb == mode_4)
        {
            const uint32_t chase_color = COLOR_AMBER; // warm bar/cabinet tone
            int l = 0;
            int r = charge_led.led_count - 1;

            // Inward sweep to center
            while (l <= r)
            {
                led_strip_clear_all(&charge_led);
                set_single_led(&charge_led, chase_color, l + 1);
                if (r != l) {
                    set_single_led(&charge_led, chase_color, r + 1);
                }
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
                l++;
                r--;
            }

            // Outward sweep back to edges
            l = charge_led.led_count / 2 - 1;
            r = (charge_led.led_count % 2 == 0) ? charge_led.led_count / 2 : charge_led.led_count / 2 + 1;
            while (l >= 0 && r < charge_led.led_count)
            {
                led_strip_clear_all(&charge_led);
                set_single_led(&charge_led, chase_color, l + 1);
                set_single_led(&charge_led, chase_color, r + 1);
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
                l--;
                r++;
            }
        }
        if (mode_rgb == mode_5)
        {
            const uint32_t chase_color = COLOR_AMBER;
            const int tail_decay = 200;      // bigger = shorter tail; adjust for preference
            const int delay_ms_step = 500;    

            int l = 0;
            int r = charge_led.led_count - 1;

            while (l <= r)
            {
                render_two_head_fade(&charge_led, chase_color, l, r, tail_decay);
                vTaskDelay(pdMS_TO_TICKS(delay_ms_step));
                l++;
                r--;
            }

            l = charge_led.led_count / 2 - 1;
            r = (charge_led.led_count % 2 == 0) ? charge_led.led_count / 2 : charge_led.led_count / 2 + 1;
            while (l >= 0 || r < charge_led.led_count)
            {
                int hl = (l >= 0) ? l : -1;
                int hr = (r < charge_led.led_count) ? r : -1;
                render_two_head_fade(&charge_led, chase_color, hl, hr, tail_decay);
                vTaskDelay(pdMS_TO_TICKS(delay_ms_step));
                l--;
                r++;
            }
        }
    }
}