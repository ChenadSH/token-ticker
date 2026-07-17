#include "app_bootstrap.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "app_key_input.h"
#include "app_sleep_schedule.h"
#include "time_service.h"
#include "ui_app.h"
#include "wifi_platform.h"

static const char *TAG = "app_bootstrap";
static const uint32_t WIFI_BOOTSTRAP_WAIT_TIMEOUT_MS = 10000;
static const uint32_t NTP_BOOTSTRAP_WAIT_TIMEOUT_MS = 10000;

static void app_bootstrap_configure_runtime_power_management(bool enable_light_sleep)
{
#if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = 40,
        .light_sleep_enable = enable_light_sleep,
    };
    esp_err_t error = esp_pm_configure(&pm_config);
    if (error == ESP_OK)
    {
        ESP_LOGI(TAG,
                 "runtime power management enabled max=%dMHz min=%dMHz light_sleep=%s",
                 pm_config.max_freq_mhz,
                 pm_config.min_freq_mhz,
                 pm_config.light_sleep_enable ? "on" : "off");
    }
    else
    {
        ESP_LOGW(TAG, "esp_pm_configure failed err=%s", esp_err_to_name(error));
    }
#endif
}

static bool app_bootstrap_is_button_pressed(const board_config_t *board, int gpio_num)
{
    gpio_config_t config = {0};
    int level;

    if (board == NULL || gpio_num < 0)
    {
        return false;
    }

    config.pin_bit_mask = 1ULL << gpio_num;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = board->buttons.active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLDOWN_DISABLE;
    config.pull_down_en = board->buttons.active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLUP_ENABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&config) != ESP_OK)
    {
        return false;
    }

    level = gpio_get_level((gpio_num_t)gpio_num);
    return board->buttons.active_low ? (level == 0) : (level != 0);
}

static bool app_bootstrap_should_allow_automatic_light_sleep(const board_config_t *board)
{
    esp_reset_reason_t reset_reason = esp_reset_reason();

    if (app_bootstrap_is_button_pressed(board, board->buttons.user_gpio))
    {
        ESP_LOGW(TAG, "auto light sleep bypassed: user key held at boot");
        return false;
    }

    switch (reset_reason)
    {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        ESP_LOGW(TAG, "auto light sleep bypassed: previous reset reason=%d", (int)reset_reason);
        return false;
    default:
        return true;
    }
}

static void app_bootstrap_configure_light_sleep_wakeup(const board_config_t *board)
{
    esp_err_t error;
    gpio_int_type_t wake_type;

    if (board == NULL)
    {
        return;
    }

    wake_type = board->buttons.active_low ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
    error = gpio_wakeup_enable((gpio_num_t)board->buttons.user_gpio, wake_type);
    if (error != ESP_OK)
    {
        ESP_LOGW(TAG, "gpio_wakeup_enable failed gpio=%d err=%s",
                 board->buttons.user_gpio,
                 esp_err_to_name(error));
        return;
    }

    error = esp_sleep_enable_gpio_wakeup();
    if (error != ESP_OK)
    {
        ESP_LOGW(TAG, "esp_sleep_enable_gpio_wakeup failed err=%s", esp_err_to_name(error));
        return;
    }

    ESP_LOGI(TAG, "light sleep wake configured on KEY gpio=%d", board->buttons.user_gpio);
}

static bool app_epoch_to_rtc_time(int64_t epoch_seconds, rtc_time_t *rtc_time)
{
    time_t raw_time = (time_t)epoch_seconds;
    struct tm time_info;

    if (rtc_time == NULL || epoch_seconds <= 0)
    {
        return false;
    }

    memset(&time_info, 0, sizeof(time_info));
    if (localtime_r(&raw_time, &time_info) == NULL)
    {
        return false;
    }

    rtc_time->year = (uint16_t)(time_info.tm_year + 1900);
    rtc_time->month = (uint8_t)(time_info.tm_mon + 1);
    rtc_time->day = (uint8_t)time_info.tm_mday;
    rtc_time->hour = (uint8_t)time_info.tm_hour;
    rtc_time->minute = (uint8_t)time_info.tm_min;
    rtc_time->second = (uint8_t)time_info.tm_sec;
    rtc_time->valid = true;
    return true;
}

static void app_bootstrap_set_runtime_mode(app_bootstrap_context_t *context, app_runtime_mode_t runtime_mode)
{
    if (context == NULL)
    {
        return;
    }

    context->runtime_mode = runtime_mode;
    ui_boot_model_set_runtime_mode(&context->ui_boot_model, runtime_mode);
}

static void app_bootstrap_log_wakeup_cause(void)
{
    ESP_LOGI(TAG, "wakeup cause=%d reset reason=%d",
             (int)esp_sleep_get_wakeup_cause(),
             (int)esp_reset_reason());
}

static void app_bootstrap_apply_initial_sleep_schedule(app_bootstrap_context_t *context)
{
    bool active = true;
    esp_sleep_wakeup_cause_t wakeup_cause;

    if (context == NULL)
    {
        return;
    }

    if (!context->rtc_time.valid || !app_sleep_schedule_is_enabled(&context->config))
    {
        app_bootstrap_set_runtime_mode(context, APP_RUNTIME_MODE_SCHEDULED_ACTIVE);
        return;
    }

    wakeup_cause = esp_sleep_get_wakeup_cause();

    if (app_sleep_schedule_is_active_now(&context->config, &context->rtc_time, &active) && !active)
    {
        if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1)
        {
            app_bootstrap_set_runtime_mode(context, APP_RUNTIME_MODE_WAKING);
            return;
        }

        app_bootstrap_set_runtime_mode(context, APP_RUNTIME_MODE_SCHEDULED_SLEEP);
        return;
    }

    app_bootstrap_set_runtime_mode(context, APP_RUNTIME_MODE_SCHEDULED_ACTIVE);
}

static const char *app_bootstrap_config_source_label(config_store_load_result_t result)
{
    switch (result)
    {
    case CONFIG_STORE_LOAD_FROM_SD:
        return "SD";
    case CONFIG_STORE_LOAD_FROM_NVS:
        return "NVS";
    case CONFIG_STORE_LOAD_SD_OVERRIDES_NVS:
        return "SD>NVS";
    case CONFIG_STORE_LOAD_UNPROVISIONED:
        return "SETUP";
    case CONFIG_STORE_LOAD_ERROR:
    default:
        return "ERROR";
    }
}

static void app_bootstrap_update_ui_network_status(app_bootstrap_context_t *context)
{
    int8_t rssi_dbm = 0;
    bool has_rssi;

    if (context == NULL)
    {
        return;
    }

    has_rssi = wifi_platform_get_rssi_dbm(&rssi_dbm);
    ui_boot_model_set_wifi_status(&context->ui_boot_model, wifi_platform_is_ready(), has_rssi, rssi_dbm);
}

void app_bootstrap_context_init(app_bootstrap_context_t *context)
{
    if (context == NULL)
    {
        return;
    }

    memset(context, 0, sizeof(*context));
    app_config_init(&context->config);
    context->allow_automatic_light_sleep = false;
    context->runtime_mode = APP_RUNTIME_MODE_SCHEDULED_ACTIVE;
    context->manual_override_until_monotonic = 0;
    context->manual_override_until_epoch = 0;
}

bool app_bootstrap_run(const board_config_t *board, app_bootstrap_context_t *context)
{
    if (board == NULL || context == NULL)
    {
        return false;
    }

    app_bootstrap_context_init(context);
    app_bootstrap_log_wakeup_cause();
    ui_app_show_boot_status(board, "BOOTING", "loading configuration", 10);

    config_store_init(board);
    if (!config_store_load_effective(&context->config, &context->config_state))
    {
        ui_app_show_boot_status(board, "BOOT FAILED", "configuration load failed", 100);
        ESP_LOGE(TAG, "config load failed");
        return false;
    }
    config_store_release_transient_resources();

    {
        char config_detail[48];

        snprintf(config_detail,
                 sizeof(config_detail),
                 "config source: %s",
                 app_bootstrap_config_source_label(context->config_state.result));
        ui_app_show_boot_status(board, "BOOTING", config_detail, 20);
    }

    ui_boot_model_init(&context->ui_boot_model, board, &context->config, &context->config_state);
    app_bootstrap_set_runtime_mode(context, APP_RUNTIME_MODE_SCHEDULED_ACTIVE);
    app_bootstrap_update_ui_network_status(context);
    app_scheduler_init(&context->scheduler_state);
    context->allow_automatic_light_sleep = app_bootstrap_should_allow_automatic_light_sleep(board);
    ESP_LOGI(TAG,
             "config result=%d sd=%s nvs=%s configured=%s image_url=%s image_refresh=%us",
             (int)context->config_state.result,
             context->config_state.sd_available ? "yes" : "no",
             context->config_state.nvs_available ? "yes" : "no",
             context->ui_boot_model.configured ? "yes" : "no",
             context->config.display.image_url,
             (unsigned)context->config.display.image_refresh_seconds);

    power_service_init(board);
    sensor_service_init(board);
    (void)app_key_input_init(board);
    time_service_init(context->config.device.ntp_enabled,
                      context->config.device.ntp_sync_hours,
                      context->config.device.timezone);
    app_bootstrap_configure_runtime_power_management(false);

    if (sensor_service_read_rtc(&context->rtc_time))
    {
        ui_boot_model_set_rtc(&context->ui_boot_model, &context->rtc_time);
        context->rtc_last_read_monotonic = (int64_t)(esp_timer_get_time() / 1000000ULL);
        app_scheduler_note_clock_rendered(&context->scheduler_state, esp_timer_get_time() / 1000000);
        app_bootstrap_apply_initial_sleep_schedule(context);
    }

    if (context->runtime_mode == APP_RUNTIME_MODE_SCHEDULED_SLEEP)
    {
        ui_app_show_boot_status(board, "BOOTING", "entering sleep", 95);
        return true;
    }

    ui_app_show_boot_status(board, "BOOTING", "preparing network", 30);

    if (wifi_platform_ensure_ready(&context->config, WIFI_BOOTSTRAP_WAIT_TIMEOUT_MS))
    {
        ESP_LOGI(TAG, "wifi status=%s", wifi_platform_status_text());
        app_bootstrap_update_ui_network_status(context);

        if (wifi_platform_is_ready() && time_service_should_sync_ntp(0))
        {
            ui_app_show_boot_status(board, "BOOTING", "syncing time", 45);
            int64_t synced_epoch;
            const int64_t monotonic_seconds = esp_timer_get_time() / 1000000;

            if (time_service_sync_ntp(NTP_BOOTSTRAP_WAIT_TIMEOUT_MS, monotonic_seconds, &synced_epoch) &&
                app_epoch_to_rtc_time(synced_epoch, &context->rtc_time))
            {
                (void)sensor_service_write_rtc(&context->rtc_time);
                ui_boot_model_set_rtc(&context->ui_boot_model, &context->rtc_time);
                context->rtc_last_read_monotonic = monotonic_seconds;
                app_scheduler_note_clock_rendered(&context->scheduler_state, esp_timer_get_time() / 1000000);
            }

            app_scheduler_note_ntp_attempted(&context->scheduler_state, monotonic_seconds);
        }
    }
    else
    {
        ESP_LOGI(TAG, "wifi skipped status=%s", wifi_platform_status_text());
    }

    if (power_service_get_status(&context->power_status))
    {
        ui_boot_model_set_power(&context->ui_boot_model, &context->power_status);
        app_scheduler_note_power_polled(&context->scheduler_state, esp_timer_get_time() / 1000000);
        ESP_LOGI(TAG,
                 "battery=%ldmV %u%%",
                 (long)context->power_status.battery_mv,
                 context->power_status.battery_percent);
    }

    ui_app_show_boot_status(board, "BOOTING", "reading sensors", 80);

    if (sensor_service_read_environment(&context->environment))
    {
        ui_boot_model_set_environment(&context->ui_boot_model, &context->environment);
        app_scheduler_note_environment_polled(&context->scheduler_state, esp_timer_get_time() / 1000000);
        ESP_LOGI(TAG,
                 "env=%d.%dC %d.%d%%",
                 context->environment.temperature_c_x10 / 10,
                 context->environment.temperature_c_x10 % 10,
                 context->environment.humidity_rh_x10 / 10,
                 context->environment.humidity_rh_x10 % 10);
    }

    if (sensor_service_read_rtc(&context->rtc_time))
    {
        ui_boot_model_set_rtc(&context->ui_boot_model, &context->rtc_time);
        context->rtc_last_read_monotonic = (int64_t)(esp_timer_get_time() / 1000000ULL);
        app_scheduler_note_clock_rendered(&context->scheduler_state, esp_timer_get_time() / 1000000);
        app_bootstrap_apply_initial_sleep_schedule(context);
        ESP_LOGI(TAG,
                 "rtc=%04u-%02u-%02u %02u:%02u:%02u",
                 context->rtc_time.year,
                 context->rtc_time.month,
                 context->rtc_time.day,
                 context->rtc_time.hour,
                 context->rtc_time.minute,
                 context->rtc_time.second);
    }

    if (context->config.wifi.enabled &&
        !app_config_image_refresh_uses_wifi_duty_cycle(&context->config))
    {
        (void)wifi_platform_stop();
        app_bootstrap_update_ui_network_status(context);
    }

    ui_app_show_boot_status(board, "BOOTING", "drawing home screen", 95);

    return true;
}

void app_bootstrap_enable_automatic_light_sleep(const app_bootstrap_context_t *context)
{
    if (context == NULL)
    {
        return;
    }

#if CONFIG_PM_ENABLE
    if (!context->allow_automatic_light_sleep)
    {
        ESP_LOGW(TAG, "automatic light sleep remains disabled for this boot");
        return;
    }

    app_bootstrap_configure_light_sleep_wakeup(board_get_config());
    app_bootstrap_configure_runtime_power_management(true);
#endif
}
