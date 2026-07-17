#include "ui_image_overlay.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"

static const lv_font_t *FONT_OVERLAY = &lv_font_montserrat_14;

#define UI_HOME_BATTERY_OUTLINE_W 24
#define UI_HOME_BATTERY_OUTLINE_H 12
#define UI_HOME_BATTERY_INNER_W 20
#define UI_HOME_BATTERY_INNER_H 8
#define UI_HOME_BATTERY_FILL_W 18
#define UI_HOME_BATTERY_FILL_H 6
#define UI_HOME_BATTERY_TIP_W 2
#define UI_HOME_BATTERY_TIP_H 4

#define UI_HOME_BATTERY_TIP_RIGHT_X 388
#define UI_HOME_BATTERY_OUTLINE_RIGHT (UI_HOME_BATTERY_TIP_RIGHT_X - UI_HOME_BATTERY_TIP_W)
#define UI_HOME_BATTERY_OUTLINE_LEFT (UI_HOME_BATTERY_OUTLINE_RIGHT - UI_HOME_BATTERY_OUTLINE_W)
#define UI_HOME_BATTERY_OUTLINE_TOP 8
#define UI_HOME_BATTERY_INNER_LEFT (UI_HOME_BATTERY_OUTLINE_LEFT + 2)
#define UI_HOME_BATTERY_INNER_TOP (UI_HOME_BATTERY_OUTLINE_TOP + 2)
#define UI_HOME_BATTERY_FILL_LEFT (UI_HOME_BATTERY_INNER_LEFT + 1)
#define UI_HOME_BATTERY_FILL_TOP (UI_HOME_BATTERY_INNER_TOP + 1)
#define UI_HOME_BATTERY_TIP_LEFT (UI_HOME_BATTERY_OUTLINE_RIGHT)
#define UI_HOME_BATTERY_TIP_TOP (UI_HOME_BATTERY_INNER_TOP)
#define UI_HOME_BATTERY_PCT_W 60
#define UI_HOME_BATTERY_PCT_RIGHT (UI_HOME_BATTERY_OUTLINE_LEFT - 6)
#define UI_HOME_BATTERY_PCT_LEFT (UI_HOME_BATTERY_PCT_RIGHT - UI_HOME_BATTERY_PCT_W)

#define UI_HOME_DATE_LEFT 8
#define UI_HOME_DATE_W 46
#define UI_HOME_TIME_LEFT (UI_HOME_DATE_LEFT + UI_HOME_DATE_W + 6)
#define UI_HOME_TIME_W 60
#define UI_HOME_STALE_W 50
#define UI_HOME_STALE_RIGHT (UI_HOME_TIME_LEFT + UI_HOME_TIME_W + 6)
#define UI_HOME_STALE_LEFT (UI_HOME_STALE_RIGHT - UI_HOME_STALE_W)
#define UI_HOME_NEXT_REFRESH_W 110
/* Park the countdown in the empty band between time and the battery strip
   (x=126..302). Anchoring it to UI_HOME_STALE_LEFT would put the label
   inside the time region (x=76) and let the time widget paint over it. */
#define UI_HOME_NEXT_REFRESH_LEFT (UI_HOME_STALE_RIGHT)
#define UI_HOME_NEXT_REFRESH_TOP 6

typedef struct
{
    bool initialized;
    lv_obj_t *date_label;
    lv_obj_t *time_label;
    lv_obj_t *stale_label;
    lv_obj_t *next_refresh_label;
    lv_obj_t *battery_pct_label;
    lv_obj_t *battery_outline;
    lv_obj_t *battery_fill;
    lv_obj_t *battery_tip;
} ui_image_overlay_state_t;

static ui_image_overlay_state_t s_overlay;

static void ui_image_overlay_set_style(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
}

static lv_obj_t *ui_image_create_label(lv_obj_t *parent,
                                      const lv_font_t *font,
                                      lv_color_t text_color,
                                      lv_coord_t x,
                                      lv_coord_t y,
                                      lv_coord_t w,
                                      lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_pos(label, x, y);
    if (w > 0)
    {
        lv_obj_set_size(label, w, LV_SIZE_CONTENT);
    }
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, text_color, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, "");
    return label;
}

static lv_obj_t *ui_image_create_filled_box(lv_obj_t *parent,
                                           lv_color_t fill_color,
                                           lv_coord_t x,
                                           lv_coord_t y,
                                           lv_coord_t w,
                                           lv_coord_t h)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_bg_color(box, fill_color, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_shadow_width(box, 0, 0);
    lv_obj_set_style_radius(box, 0, 0);
    return box;
}

static lv_obj_t *ui_image_create_battery_outline(lv_obj_t *parent,
                                                 lv_coord_t x,
                                                 lv_coord_t y)
{
    lv_obj_t *outline = lv_obj_create(parent);
    lv_obj_set_pos(outline, x, y);
    lv_obj_set_size(outline, UI_HOME_BATTERY_OUTLINE_W, UI_HOME_BATTERY_OUTLINE_H);
    lv_obj_set_style_bg_color(outline, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(outline, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(outline, lv_color_white(), 0);
    lv_obj_set_style_border_width(outline, 2, 0);
    lv_obj_set_style_radius(outline, 0, 0);
    lv_obj_set_style_pad_all(outline, 0, 0);
    lv_obj_set_style_shadow_width(outline, 0, 0);
    return outline;
}

bool ui_image_overlay_init(void)
{
    lv_obj_t *screen;

    if (s_overlay.initialized)
    {
        return true;
    }

    memset(&s_overlay, 0, sizeof(s_overlay));
    screen = lv_screen_active();
    if (screen == NULL)
    {
        return false;
    }

    s_overlay.date_label = ui_image_create_label(screen,
                                               FONT_OVERLAY,
                                               lv_color_white(),
                                               UI_HOME_DATE_LEFT,
                                               6,
                                               UI_HOME_DATE_W,
                                               LV_TEXT_ALIGN_LEFT);
    s_overlay.time_label = ui_image_create_label(screen,
                                               FONT_OVERLAY,
                                               lv_color_white(),
                                               UI_HOME_TIME_LEFT,
                                               6,
                                               UI_HOME_TIME_W,
                                               LV_TEXT_ALIGN_LEFT);
    s_overlay.stale_label = ui_image_create_label(screen,
                                                 FONT_OVERLAY,
                                                 lv_color_white(),
                                                 UI_HOME_STALE_LEFT,
                                                 6,
                                                 UI_HOME_STALE_W,
                                                 LV_TEXT_ALIGN_LEFT);

    s_overlay.next_refresh_label = ui_image_create_label(screen,
                                                         FONT_OVERLAY,
                                                         lv_color_white(),
                                                         UI_HOME_NEXT_REFRESH_LEFT,
                                                         UI_HOME_NEXT_REFRESH_TOP,
                                                         UI_HOME_NEXT_REFRESH_W,
                                                         LV_TEXT_ALIGN_LEFT);

    s_overlay.battery_pct_label = ui_image_create_label(screen,
                                                     FONT_OVERLAY,
                                                     lv_color_white(),
                                                     UI_HOME_BATTERY_PCT_LEFT,
                                                     6,
                                                     UI_HOME_BATTERY_PCT_W,
                                                     LV_TEXT_ALIGN_RIGHT);

    s_overlay.battery_outline = ui_image_create_battery_outline(screen,
                                                               UI_HOME_BATTERY_OUTLINE_LEFT,
                                                               UI_HOME_BATTERY_OUTLINE_TOP);
    s_overlay.battery_fill = ui_image_create_filled_box(screen,
                                                       lv_color_white(),
                                                       UI_HOME_BATTERY_FILL_LEFT,
                                                       UI_HOME_BATTERY_FILL_TOP,
                                                       0,
                                                       UI_HOME_BATTERY_FILL_H);
    s_overlay.battery_tip = ui_image_create_filled_box(screen,
                                                    lv_color_white(),
                                                    UI_HOME_BATTERY_TIP_LEFT,
                                                    UI_HOME_BATTERY_TIP_TOP,
                                                    UI_HOME_BATTERY_TIP_W,
                                                    UI_HOME_BATTERY_TIP_H);

    s_overlay.initialized = true;
    return true;
}

void ui_image_overlay_reset(void)
{
    memset(&s_overlay, 0, sizeof(s_overlay));
}

void ui_image_overlay_mark_stale(bool stale)
{
    if (s_overlay.stale_label == NULL)
    {
        return;
    }
    if (stale)
    {
        lv_label_set_text(s_overlay.stale_label, "STALE");
    }
    else
    {
        lv_label_set_text(s_overlay.stale_label, "");
    }
}

void ui_image_overlay_invalidate(void)
{
    /* Force every overlay widget to be redrawn. This is needed before pushing
       a freshly fetched image because some widgets (the battery outline and
       tip are never touched after init, and the battery fill keeps the same
       width when the percentage hasn't changed) would otherwise be skipped by
       LVGL's dirty tracking, leaving the overlay missing on top of the BMP. */
    if (s_overlay.date_label != NULL) lv_obj_invalidate(s_overlay.date_label);
    if (s_overlay.time_label != NULL) lv_obj_invalidate(s_overlay.time_label);
    if (s_overlay.stale_label != NULL) lv_obj_invalidate(s_overlay.stale_label);
    if (s_overlay.next_refresh_label != NULL) lv_obj_invalidate(s_overlay.next_refresh_label);
    if (s_overlay.battery_pct_label != NULL) lv_obj_invalidate(s_overlay.battery_pct_label);
    if (s_overlay.battery_outline != NULL) lv_obj_invalidate(s_overlay.battery_outline);
    if (s_overlay.battery_fill != NULL) lv_obj_invalidate(s_overlay.battery_fill);
    if (s_overlay.battery_tip != NULL) lv_obj_invalidate(s_overlay.battery_tip);
}

static void ui_image_overlay_show_widgets(bool show)
{
    if (show)
    {
        if (s_overlay.date_label != NULL) lv_obj_clear_flag(s_overlay.date_label, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.time_label != NULL) lv_obj_clear_flag(s_overlay.time_label, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.stale_label != NULL) lv_obj_clear_flag(s_overlay.stale_label, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.next_refresh_label != NULL) lv_obj_clear_flag(s_overlay.next_refresh_label, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.battery_pct_label != NULL) lv_obj_clear_flag(s_overlay.battery_pct_label, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.battery_outline != NULL) lv_obj_clear_flag(s_overlay.battery_outline, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.battery_fill != NULL) lv_obj_clear_flag(s_overlay.battery_fill, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.battery_tip != NULL) lv_obj_clear_flag(s_overlay.battery_tip, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        if (s_overlay.date_label != NULL) lv_obj_add_flag(s_overlay.date_label, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.time_label != NULL) lv_obj_add_flag(s_overlay.time_label, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.stale_label != NULL) lv_obj_add_flag(s_overlay.stale_label, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.next_refresh_label != NULL) lv_obj_add_flag(s_overlay.next_refresh_label, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.battery_pct_label != NULL) lv_obj_add_flag(s_overlay.battery_pct_label, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.battery_outline != NULL) lv_obj_add_flag(s_overlay.battery_outline, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.battery_fill != NULL) lv_obj_add_flag(s_overlay.battery_fill, LV_OBJ_FLAG_HIDDEN);
        if (s_overlay.battery_tip != NULL) lv_obj_add_flag(s_overlay.battery_tip, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_image_overlay_update(const ui_image_overlay_t *overlay)
{
    char pct_text[UI_IMAGE_OVERLAY_TEXT_LEN];

    if (overlay == NULL)
    {
        return;
    }

    if (!overlay->has_time && !overlay->has_battery)
    {
        ui_image_overlay_show_widgets(false);
        return;
    }
    ui_image_overlay_show_widgets(true);

    if (s_overlay.date_label != NULL)
    {
        lv_label_set_text(s_overlay.date_label,
                          overlay->date_text[0] != '\0' ? overlay->date_text : "");
    }
    if (s_overlay.time_label != NULL)
    {
        lv_label_set_text(s_overlay.time_label,
                          overlay->time_text[0] != '\0' ? overlay->time_text : "");
    }
    if (s_overlay.next_refresh_label != NULL)
    {
        if (overlay->has_next_refresh)
        {
            lv_label_set_text(s_overlay.next_refresh_label,
                              overlay->next_refresh_text[0] != '\0' ? overlay->next_refresh_text : "");
        }
        else
        {
            lv_label_set_text(s_overlay.next_refresh_label, "");
        }
    }
    if (s_overlay.battery_pct_label != NULL)
    {
        if (overlay->has_battery)
        {
            if (overlay->battery_charging)
            {
                snprintf(pct_text, sizeof(pct_text), "CHG %u%%", (unsigned)overlay->battery_percent);
            }
            else
            {
                snprintf(pct_text, sizeof(pct_text), "%u%%", (unsigned)overlay->battery_percent);
            }
            lv_label_set_text(s_overlay.battery_pct_label, pct_text);
        }
        else
        {
            lv_label_set_text(s_overlay.battery_pct_label, "");
        }
    }
    if (s_overlay.battery_fill != NULL)
    {
        if (overlay->has_battery)
        {
            int width = (overlay->battery_percent * UI_HOME_BATTERY_FILL_W) / 100;
            if (overlay->battery_percent == 0)
            {
                width = 0;
            }
            else if (width < 1)
            {
                width = 1;
            }
            else if (width > UI_HOME_BATTERY_FILL_W)
            {
                width = UI_HOME_BATTERY_FILL_W;
            }
            lv_obj_set_size(s_overlay.battery_fill, (lv_coord_t)width, UI_HOME_BATTERY_FILL_H);
        }
        else
        {
            lv_obj_set_size(s_overlay.battery_fill, 0, UI_HOME_BATTERY_FILL_H);
        }
    }
}
