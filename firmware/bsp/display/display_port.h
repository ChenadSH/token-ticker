#ifndef TOKEN_TICKER_DISPLAY_PORT_H
#define TOKEN_TICKER_DISPLAY_PORT_H

#include <stdbool.h>

#include "board.h"
#include "lvgl.h"

bool display_port_init(const board_config_t *board);
lv_display_t *display_port_get_display(void);
void display_port_render(void);
bool display_port_set_sleep(bool sleep);
bool display_port_write_bitmap(const uint8_t *bitmap, size_t bitmap_len);
bool display_port_render_bmp(const uint8_t *bmp, size_t bmp_size);

#endif