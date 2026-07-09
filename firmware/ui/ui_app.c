#include "ui_app.h"

#include <stdio.h>

#include "display_port.h"
#include "esp_log.h"

#include "ui_boot_model.h"
#include "ui_image_overlay.h"
#include "ui_sleep_screen.h"

static const char *TAG = "token_ticker_ui";

typedef enum
{
    UI_APP_SCENE_NONE = 0,
    UI_APP_SCENE_HOME,
    UI_APP_SCENE_SLEEP,
    UI_APP_SCENE_WAKING,
} ui_app_scene_t;

static bool s_boot_screen_ready;
static bool s_ui_ready;
static ui_app_scene_t s_scene;
static lv_obj_t *s_boot_title_label;
static lv_obj_t *s_boot_phase_label;
static lv_obj_t *s_boot_detail_label;
static lv_obj_t *s_boot_progress_label;
static lv_obj_t *s_boot_progress_bar;

static bool ui_app_boot_screen_init(const board_config_t *board)
{
    lv_obj_t *screen;

    if (board == NULL)
    {
        return false;
    }

    if (!display_port_init(board))
    {
        return false;
    }

    if (s_boot_screen_ready)
    {
        return true;
    }

    screen = lv_screen_active();
    if (screen == NULL)
    {
        return false;
    }

    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    s_boot_title_label = lv_label_create(screen);
    lv_label_set_text(s_boot_title_label, "TOKEN TICKER");
    lv_obj_set_style_text_font(s_boot_title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(s_boot_title_label, lv_color_white(), 0);
    lv_obj_align(s_boot_title_label, LV_ALIGN_TOP_MID, 0, 28);

    s_boot_phase_label = lv_label_create(screen);
    lv_label_set_text(s_boot_phase_label, "BOOTING");
    lv_obj_set_style_text_font(s_boot_phase_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(s_boot_phase_label, lv_color_white(), 0);
    lv_obj_set_width(s_boot_phase_label, 320);
    lv_obj_set_style_text_align(s_boot_phase_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_boot_phase_label, LV_ALIGN_TOP_MID, 0, 92);

    s_boot_detail_label = lv_label_create(screen);
    lv_label_set_text(s_boot_detail_label, "starting");
    lv_obj_set_style_text_font(s_boot_detail_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_boot_detail_label, lv_color_white(), 0);
    lv_obj_set_width(s_boot_detail_label, 340);
    lv_obj_set_style_text_align(s_boot_detail_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_boot_detail_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_boot_detail_label, LV_ALIGN_TOP_MID, 0, 132);

    s_boot_progress_bar = lv_bar_create(screen);
    lv_obj_set_size(s_boot_progress_bar, 300, 18);
    lv_obj_align(s_boot_progress_bar, LV_ALIGN_BOTTOM_MID, 0, -52);
    lv_bar_set_range(s_boot_progress_bar, 0, 100);
    lv_obj_set_style_bg_color(s_boot_progress_bar, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_boot_progress_bar, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_boot_progress_bar, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_boot_progress_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_boot_progress_bar, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(s_boot_progress_bar, 2, LV_PART_MAIN);
    lv_bar_set_value(s_boot_progress_bar, 0, LV_ANIM_OFF);

    s_boot_progress_label = lv_label_create(screen);
    lv_label_set_text(s_boot_progress_label, "0%");
    lv_obj_set_style_text_font(s_boot_progress_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_boot_progress_label, lv_color_white(), 0);
    lv_obj_align(s_boot_progress_label, LV_ALIGN_BOTTOM_MID, 0, -26);

    s_boot_screen_ready = true;
    return true;
}

static void ui_app_render_home(const ui_boot_model_t *model)
{
    ui_image_overlay_t overlay;

    if (model == NULL)
    {
        return;
    }

    memset(&overlay, 0, sizeof(overlay));
    overlay.has_time = model->rtc_time.valid;
    if (overlay.has_time)
    {
        snprintf(overlay.date_text,
                 sizeof(overlay.date_text),
                 "%02u-%02u",
                 (unsigned)model->rtc_time.month,
                 (unsigned)model->rtc_time.day);
        snprintf(overlay.time_text,
                 sizeof(overlay.time_text),
                 "%02u:%02u",
                 (unsigned)model->rtc_time.hour,
                 (unsigned)model->rtc_time.minute);
    }
    if (model->power.valid)
    {
        overlay.has_battery = true;
        overlay.battery_percent = model->power.battery_percent;
        overlay.battery_charging = (model->power.charge_state == POWER_CHARGE_STATE_CHARGING);
    }
    overlay.has_image = model->has_image_loaded_epoch;
    overlay.image_stale = false;

    ui_image_overlay_update(&overlay);
    display_port_render();
}

static bool ui_app_show_home_scene(void)
{
    if (s_scene != UI_APP_SCENE_HOME)
    {
        lv_obj_t *screen = lv_screen_active();
        if (screen != NULL)
        {
            lv_obj_clean(screen);
        }
        (void)display_port_set_sleep(false);
        ui_image_overlay_reset();
        if (!ui_image_overlay_init())
        {
            return false;
        }

        s_scene = UI_APP_SCENE_HOME;
    }

    return true;
}

static bool ui_app_show_sleep_scene(void)
{
    if (s_scene != UI_APP_SCENE_SLEEP)
    {
        (void)display_port_set_sleep(false);
        ui_image_overlay_reset();
        if (!ui_sleep_screen_show())
        {
            return false;
        }

        display_port_render();
        (void)display_port_set_sleep(true);
        s_scene = UI_APP_SCENE_SLEEP;
    }

    return true;
}

static bool ui_app_show_waking_scene(void)
{
    if (s_scene != UI_APP_SCENE_WAKING)
    {
        (void)display_port_set_sleep(false);
        ui_image_overlay_reset();
        if (!ui_sleep_screen_show_waking())
        {
            return false;
        }

        display_port_render();
        s_scene = UI_APP_SCENE_WAKING;
    }

    return true;
}

static void ui_app_render_scene(const ui_boot_model_t *model)
{
    if (model == NULL)
    {
        return;
    }

    if (model->runtime_mode == APP_RUNTIME_MODE_SCHEDULED_SLEEP)
    {
        if (!ui_app_show_sleep_scene())
        {
            ESP_LOGE(TAG, "sleep screen init failed");
        }
        return;
    }

    if (model->runtime_mode == APP_RUNTIME_MODE_WAKING)
    {
        if (!ui_app_show_waking_scene())
        {
            ESP_LOGE(TAG, "waking screen init failed");
        }
        return;
    }

    if (!ui_app_show_home_scene())
    {
        ESP_LOGE(TAG, "home screen init failed");
        return;
    }

    ui_app_render_home(model);
}

void ui_app_show_boot_status(const board_config_t *board,
                             const char *phase,
                             const char *detail,
                             uint8_t progress_percent)
{
    char progress_text[8];

    if (!ui_app_boot_screen_init(board))
    {
        ESP_LOGW(TAG, "boot status skipped: display unavailable");
        return;
    }

    if (progress_percent > 100)
    {
        progress_percent = 100;
    }

    if (phase != NULL)
    {
        lv_label_set_text(s_boot_phase_label, phase);
    }
    if (detail != NULL)
    {
        lv_label_set_text(s_boot_detail_label, detail);
    }

    snprintf(progress_text, sizeof(progress_text), "%u%%", (unsigned)progress_percent);
    lv_label_set_text(s_boot_progress_label, progress_text);
    lv_bar_set_value(s_boot_progress_bar, progress_percent, LV_ANIM_OFF);
    display_port_render();
}

void ui_app_boot(const ui_boot_model_t *model)
{
    if (model == NULL || model->board == NULL)
    {
        ESP_LOGW(TAG, "ui boot skipped: model unavailable");
        return;
    }

    if (!display_port_init(model->board))
    {
        ESP_LOGE(TAG, "display port init failed");
        return;
    }

    s_ui_ready = true;
    s_boot_screen_ready = false;
    s_scene = UI_APP_SCENE_NONE;

    ESP_LOGI(TAG, "ui boot with orientation=%s configured=%s",
             model->board->display.landscape ? "landscape" : "portrait",
             model->configured ? "yes" : "no");

    ui_app_render_scene(model);
}

void ui_app_update(const ui_boot_model_t *model)
{
    if (!s_ui_ready || model == NULL)
    {
        return;
    }

    ui_app_render_scene(model);
}
