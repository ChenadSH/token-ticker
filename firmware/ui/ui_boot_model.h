#ifndef TOKEN_TICKER_UI_BOOT_MODEL_H
#define TOKEN_TICKER_UI_BOOT_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "app_runtime_mode.h"
#include "board.h"
#include "config_store.h"
#include "power_service.h"
#include "sensor_service.h"

typedef struct
{
    const board_config_t *board;
    bool configured;
    config_store_load_result_t config_result;
    bool sd_config_available;
    bool nvs_config_available;
    bool wifi_enabled;
    bool wifi_connected;
    bool wifi_has_rssi;
    int8_t wifi_rssi_dbm;
    power_status_t power;
    environment_sample_t environment;
    rtc_time_t rtc_time;
    int64_t image_loaded_epoch;
    bool has_image_loaded_epoch;
    app_runtime_mode_t runtime_mode;
} ui_boot_model_t;

void ui_boot_model_init(ui_boot_model_t *model,
                        const board_config_t *board,
                        const app_config_t *config,
                        const config_store_state_t *config_state);
void ui_boot_model_set_power(ui_boot_model_t *model, const power_status_t *power);
void ui_boot_model_set_environment(ui_boot_model_t *model, const environment_sample_t *environment);
void ui_boot_model_set_rtc(ui_boot_model_t *model, const rtc_time_t *rtc_time);
void ui_boot_model_set_image_loaded(ui_boot_model_t *model, int64_t image_epoch_seconds);
void ui_boot_model_set_wifi_status(ui_boot_model_t *model, bool connected, bool has_rssi, int8_t rssi_dbm);
void ui_boot_model_set_runtime_mode(ui_boot_model_t *model, app_runtime_mode_t runtime_mode);

#endif
