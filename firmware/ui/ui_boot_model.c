#include "ui_boot_model.h"

#include <string.h>

void ui_boot_model_init(ui_boot_model_t *model,
                        const board_config_t *board,
                        const app_config_t *config,
                        const config_store_state_t *config_state)
{
    if (model == NULL)
    {
        return;
    }

    memset(model, 0, sizeof(*model));
    model->board = board;
    model->runtime_mode = APP_RUNTIME_MODE_SCHEDULED_ACTIVE;

    if (config != NULL)
    {
        model->configured = config_store_is_provisioned(config);
        model->wifi_enabled = config->wifi.enabled;
    }

    if (config_state != NULL)
    {
        model->config_result = config_state->result;
        model->sd_config_available = config_state->sd_available;
        model->nvs_config_available = config_state->nvs_available;
    }
}

void ui_boot_model_set_power(ui_boot_model_t *model, const power_status_t *power)
{
    if (model == NULL || power == NULL)
    {
        return;
    }

    model->power = *power;
}

void ui_boot_model_set_environment(ui_boot_model_t *model, const environment_sample_t *environment)
{
    if (model == NULL || environment == NULL)
    {
        return;
    }

    model->environment = *environment;
}

void ui_boot_model_set_rtc(ui_boot_model_t *model, const rtc_time_t *rtc_time)
{
    if (model == NULL || rtc_time == NULL)
    {
        return;
    }

    model->rtc_time = *rtc_time;
}

void ui_boot_model_set_image_loaded(ui_boot_model_t *model, int64_t image_epoch_seconds)
{
    if (model == NULL || image_epoch_seconds <= 0)
    {
        return;
    }

    model->image_loaded_epoch = image_epoch_seconds;
    model->has_image_loaded_epoch = true;
}

void ui_boot_model_set_wifi_status(ui_boot_model_t *model, bool connected, bool has_rssi, int8_t rssi_dbm)
{
    if (model == NULL)
    {
        return;
    }

    model->wifi_connected = connected;
    model->wifi_has_rssi = has_rssi;
    model->wifi_rssi_dbm = has_rssi ? rssi_dbm : 0;
}

void ui_boot_model_set_runtime_mode(ui_boot_model_t *model, app_runtime_mode_t runtime_mode)
{
    if (model == NULL)
    {
        return;
    }

    model->runtime_mode = runtime_mode;
}
