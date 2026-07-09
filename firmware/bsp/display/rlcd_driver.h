#ifndef TOKEN_TICKER_RLCD_DRIVER_H
#define TOKEN_TICKER_RLCD_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_lcd_panel_io.h"

#include "board.h"

typedef struct
{
    bool initialized;
    bool sleeping;
    int width;
    int height;
    bool landscape;
    esp_lcd_panel_io_handle_t io_handle;
    uint8_t *framebuffer;
    size_t framebuffer_len;
} rlcd_driver_t;

bool rlcd_driver_init(rlcd_driver_t *driver, const board_config_t *board);
void rlcd_driver_deinit(rlcd_driver_t *driver);
void rlcd_driver_clear(rlcd_driver_t *driver, bool white);
void rlcd_driver_set_pixel(rlcd_driver_t *driver, int x, int y, bool white);
bool rlcd_driver_write_bitmap(rlcd_driver_t *driver,
                              const uint8_t *bitmap,
                              size_t bitmap_len);
void rlcd_driver_display(rlcd_driver_t *driver);
bool rlcd_driver_set_sleep(rlcd_driver_t *driver, bool sleep);

#endif