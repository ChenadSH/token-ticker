#ifndef TOKEN_TICKER_UI_IMAGE_OVERLAY_H
#define TOKEN_TICKER_UI_IMAGE_OVERLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_IMAGE_OVERLAY_TEXT_LEN 32

typedef struct
{
    bool has_time;
    char time_text[UI_IMAGE_OVERLAY_TEXT_LEN];
    char date_text[UI_IMAGE_OVERLAY_TEXT_LEN];
    bool has_battery;
    uint8_t battery_percent;
    bool battery_charging;
    bool has_image;
    bool image_stale;
    bool has_next_refresh;
    char next_refresh_text[UI_IMAGE_OVERLAY_TEXT_LEN];
} ui_image_overlay_t;

bool ui_image_overlay_init(void);
void ui_image_overlay_reset(void);
void ui_image_overlay_update(const ui_image_overlay_t *overlay);
void ui_image_overlay_mark_stale(bool stale);
void ui_image_overlay_invalidate(void);

#endif
