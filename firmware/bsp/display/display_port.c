#include "display_port.h"

#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "bmp_to_rlcd.h"
#include "rlcd_driver.h"

static const char *TAG = "display_port";
static lv_display_t *s_display;
static void *s_buffer;
static rlcd_driver_t s_rlcd_driver;
static uint32_t s_flush_log_count;

static uint32_t display_port_tick_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void display_port_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    uint16_t *buffer = (uint16_t *)color_p;
    uint32_t black_pixels = 0;
    int y;

    for (y = area->y1; y <= area->y2; ++y)
    {
        int x;
        for (x = area->x1; x <= area->x2; ++x)
        {
            const uint16_t pixel = *buffer++;
            const bool white = pixel >= 0x7FFF;
            if (!white)
            {
                ++black_pixels;
            }
            rlcd_driver_set_pixel(&s_rlcd_driver, x, y, white);
        }
    }

    if (s_flush_log_count < 4)
    {
        ++s_flush_log_count;
        ESP_LOGI(TAG,
                 "flush[%lu] area=(%d,%d)-(%d,%d) pixels=%lu black=%lu",
                 (unsigned long)s_flush_log_count,
                 area->x1,
                 area->y1,
                 area->x2,
                 area->y2,
                 (unsigned long)((area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1)),
                 (unsigned long)black_pixels);
    }

    lv_display_flush_ready(disp);
}

bool display_port_init(const board_config_t *board)
{
    size_t buffer_pixels;
    size_t buffer_bytes;

    if (board == NULL)
    {
        return false;
    }

    if (s_display != NULL)
    {
        return true;
    }

    lv_init();
    lv_tick_set_cb(display_port_tick_cb);

    s_display = lv_display_create(board->display.display_width, board->display.display_height);
    if (s_display == NULL)
    {
        ESP_LOGE(TAG, "lv_display_create failed");
        return false;
    }

    if (!rlcd_driver_init(&s_rlcd_driver, board))
    {
        ESP_LOGE(TAG, "rlcd driver init failed");
        return false;
    }

    ESP_LOGI(TAG,
             "display port ready size=%dx%d buffer_rows=%u",
             board->display.display_width,
             board->display.display_height,
             20U);

    lv_display_set_flush_cb(s_display, display_port_flush_cb);
    buffer_pixels = (size_t)board->display.display_width * 20U;
    buffer_bytes = LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565) * buffer_pixels;
    s_buffer = heap_caps_malloc(buffer_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (s_buffer == NULL)
    {
        ESP_LOGE(TAG, "display buffer allocation failed");
        return false;
    }

    lv_display_set_buffers(s_display, s_buffer, NULL, buffer_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    return true;
}

lv_display_t *display_port_get_display(void)
{
    return s_display;
}

bool display_port_set_sleep(bool sleep)
{
    if (s_display == NULL || !s_rlcd_driver.initialized)
    {
        return false;
    }

    return rlcd_driver_set_sleep(&s_rlcd_driver, sleep);
}

bool display_port_write_bitmap(const uint8_t *bitmap, size_t bitmap_len)
{
    if (s_display == NULL || !s_rlcd_driver.initialized)
    {
        return false;
    }

    if (!rlcd_driver_write_bitmap(&s_rlcd_driver, bitmap, bitmap_len))
    {
        return false;
    }

    rlcd_driver_display(&s_rlcd_driver);
    return true;
}

bool display_port_render_bmp(const uint8_t *bmp, size_t bmp_size)
{
    if (s_display == NULL || !s_rlcd_driver.initialized)
    {
        return false;
    }

    if (!bmp_to_rlcd_write_bitmap(&s_rlcd_driver, bmp, bmp_size))
    {
        return false;
    }

    /* Compose any dirty LVGL widgets (the home-screen overlay, the sleep/waking
       screen content, etc.) on top of the freshly-written bitmap so the bare
       image is never pushed to the panel alone. */
    lv_refr_now(s_display);
    rlcd_driver_display(&s_rlcd_driver);
    return true;
}

void display_port_render(void)
{
    if (s_display != NULL && !s_rlcd_driver.sleeping)
    {
        (void)lv_timer_handler();
        rlcd_driver_display(&s_rlcd_driver);
    }
}