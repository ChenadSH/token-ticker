#ifndef TOKEN_TICKER_APP_BOOTSTRAP_H
#define TOKEN_TICKER_APP_BOOTSTRAP_H

#include <stdbool.h>

#include "app_runtime_mode.h"
#include "app_config.h"
#include "app_scheduler.h"
#include "board.h"
#include "config_store.h"
#include "power_service.h"
#include "sensor_service.h"
#include "ui_boot_model.h"

typedef struct
{
    app_config_t config;
    config_store_state_t config_state;
    bool allow_automatic_light_sleep;
    app_runtime_mode_t runtime_mode;
    int64_t manual_override_until_monotonic;
    int64_t manual_override_until_epoch;
    power_status_t power_status;
    environment_sample_t environment;
    rtc_time_t rtc_time;
    app_scheduler_state_t scheduler_state;
    ui_boot_model_t ui_boot_model;
} app_bootstrap_context_t;

void app_bootstrap_context_init(app_bootstrap_context_t *context);
bool app_bootstrap_run(const board_config_t *board, app_bootstrap_context_t *context);
void app_bootstrap_enable_automatic_light_sleep(const app_bootstrap_context_t *context);

#endif
