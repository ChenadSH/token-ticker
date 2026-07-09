#include "bmp_to_rlcd.h"

#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#include "rlcd_driver.h"

static const char *TAG = "bmp_to_rlcd";

bool bmp_to_rlcd_write_bitmap(rlcd_driver_t *driver, const uint8_t *bmp, size_t bmp_size)
{
    if (driver == NULL || driver->framebuffer == NULL)
    {
        return false;
    }
    if (bmp == NULL || bmp_size < 14)
    {
        return false;
    }
    if (bmp[0] != 'B' || bmp[1] != 'M')
    {
        ESP_LOGW(TAG, "not a BMP file (missing BM signature)");
        return false;
    }

    uint32_t data_offset = 0;
    memcpy(&data_offset, bmp + 10, 4);
    if (data_offset >= bmp_size || data_offset < 14)
    {
        ESP_LOGW(TAG, "BMP data offset out of range: offset=%u size=%u", (unsigned)data_offset, (unsigned)bmp_size);
        return false;
    }
    if (data_offset + 12 > bmp_size)
    {
        ESP_LOGW(TAG, "BMP file too small for DIB header");
        return false;
    }

    int32_t bmp_width = 0;
    int32_t bmp_height = 0;
    int16_t bpp = 0;
    memcpy(&bmp_width, bmp + 18, 4);
    memcpy(&bmp_height, bmp + 22, 4);
    memcpy(&bpp, bmp + 28, 2);

    const int width = (int)driver->width;
    const int height = (int)driver->height;

    if (bmp_width != width || bmp_height != height)
    {
        ESP_LOGW(TAG, "BMP size mismatch got=%dx%d expected=%dx%d", (int)bmp_width, (int)bmp_height, width, height);
        return false;
    }
    if (bpp != 1)
    {
        ESP_LOGW(TAG, "BMP must be 1 bpp, got=%d", (int)bpp);
        return false;
    }
    if (bmp_height <= 0)
    {
        ESP_LOGW(TAG, "only bottom-up BMP supported");
        return false;
    }

    const int bmp_row_bytes = (width + 7) / 8;
    const int bmp_row_padded = (bmp_row_bytes + 3) & ~3;
    const int block_rows = height / 4;
    const size_t fb_size = (size_t)(width / 2) * (size_t)block_rows;

    if (driver->framebuffer_len < fb_size)
    {
        ESP_LOGW(TAG, "framebuffer too small: have=%u need=%u",
                 (unsigned)driver->framebuffer_len, (unsigned)fb_size);
        return false;
    }

    memset(driver->framebuffer, 0x00, driver->framebuffer_len);

    for (int visual_y = 0; visual_y < height; visual_y++)
    {
        int bmp_row = bmp_height - 1 - visual_y;
        const uint8_t *bmp_row_ptr = bmp + data_offset + (size_t)bmp_row * (size_t)bmp_row_padded;
        if ((size_t)data_offset + (size_t)(bmp_row + 1) * (size_t)bmp_row_padded > bmp_size)
        {
            continue;
        }

        const int inv_y = height - 1 - visual_y;
        const int block_y = inv_y / 4;
        const int local_y = inv_y % 4;
        const uint8_t even_bit = (uint8_t)(7 - local_y * 2);
        const uint8_t odd_bit = (uint8_t)(6 - local_y * 2);

        for (int i = 0; i < bmp_row_bytes; i++)
        {
            const uint8_t bmp_byte = bmp_row_ptr[i];
            const int fb_col_base = i * 2;

            for (int pair = 0; pair < 4; pair++)
            {
                const int fb_byte_x = i * 4 + pair;
                const int x_even = fb_col_base + pair * 2;
                if (x_even >= width)
                {
                    break;
                }

                const size_t fb_index = (size_t)fb_byte_x * (size_t)block_rows + (size_t)block_y;
                uint8_t fb_byte = driver->framebuffer[fb_index];

                const int bmp_bit_even = 7 - 2 * pair;
                if ((bmp_byte >> bmp_bit_even) & 1)
                {
                    fb_byte |= (uint8_t)(1U << even_bit);
                }

                if (x_even + 1 < width)
                {
                    const int bmp_bit_odd = 6 - 2 * pair;
                    if ((bmp_byte >> bmp_bit_odd) & 1)
                    {
                        fb_byte |= (uint8_t)(1U << odd_bit);
                    }
                }

                driver->framebuffer[fb_index] = fb_byte;
            }
        }
    }

    /* Intentionally do not push to the panel here. The display_port layer is
       responsible for composing any LVGL overlay on top of this framebuffer
       before the final rlcd_driver_display call, so that widgets drawn via
       LVGL are not erased by an intermediate push of the bare bitmap. */
    return true;
}
