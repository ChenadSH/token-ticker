#include "app_runtime.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_key_input.h"
#include "app_sleep_schedule.h"
#include "display_port.h"
#include "http_client.h"
#include "time_service.h"
#include "ui_app.h"
#include "ui_image_overlay.h"
#include "wifi_platform.h"

static const char *TAG = "app_runtime";
static const uint32_t APP_RUNTIME_TASK_STACK_SIZE = 8192;
static const uint32_t WIFI_RUNTIME_WAIT_TIMEOUT_MS = 10000;
static const uint32_t NTP_RUNTIME_WAIT_TIMEOUT_MS = 10000;
static const uint32_t DEFAULT_MANUAL_OVERRIDE_SECONDS = 5 * 60;
static const uint32_t FALLBACK_SLEEP_WAIT_SECONDS = 60;
static const uint32_t IMAGE_FETCH_TIMEOUT_MS = 8000;
static const char *SLEEP_TAG = "app_sleep";
static const size_t IMAGE_FETCH_BUFFER_BYTES = 20480;

static void app_runtime_update_ui_network_status(app_bootstrap_context_t *context);

static void app_runtime_set_mode(app_bootstrap_context_t *context, app_runtime_mode_t runtime_mode)
{
    if (context == NULL)
    {
        return;
    }

    context->runtime_mode = runtime_mode;
    ui_boot_model_set_runtime_mode(&context->ui_boot_model, runtime_mode);
    if (runtime_mode != APP_RUNTIME_MODE_MANUAL_ACTIVE_OVERRIDE)
    {
        context->manual_override_until_monotonic = 0;
        context->manual_override_until_epoch = 0;
    }

    if (runtime_mode == APP_RUNTIME_MODE_SCHEDULED_SLEEP)
    {
        (void)wifi_platform_stop();
        app_runtime_update_ui_network_status(context);
    }
}

static uint32_t app_runtime_manual_override_seconds(const app_bootstrap_context_t *context)
{
    uint32_t minutes;

    if (context == NULL)
    {
        return DEFAULT_MANUAL_OVERRIDE_SECONDS;
    }

    minutes = context->config.device.sleep_schedule.manual_wake_minutes;
    if (minutes == 0)
    {
        return 0;
    }

    return minutes * 60U;
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

static bool app_rtc_time_to_epoch(const rtc_time_t *rtc_time, int64_t *epoch_seconds)
{
    struct tm time_info;
    time_t local_epoch;

    if (rtc_time == NULL || epoch_seconds == NULL || !rtc_time->valid)
    {
        return false;
    }

    memset(&time_info, 0, sizeof(time_info));
    time_info.tm_year = (int)rtc_time->year - 1900;
    time_info.tm_mon = (int)rtc_time->month - 1;
    time_info.tm_mday = (int)rtc_time->day;
    time_info.tm_hour = (int)rtc_time->hour;
    time_info.tm_min = (int)rtc_time->minute;
    time_info.tm_sec = (int)rtc_time->second;
    time_info.tm_isdst = -1;

    local_epoch = mktime(&time_info);
    if (local_epoch < 0)
    {
        return false;
    }

    *epoch_seconds = (int64_t)local_epoch;
    return true;
}

static bool app_rtc_add_seconds(const rtc_time_t *base_time, uint32_t delta_seconds, rtc_time_t *result)
{
    int64_t epoch_seconds;

    if (result == NULL || !app_rtc_time_to_epoch(base_time, &epoch_seconds))
    {
        return false;
    }

    return app_epoch_to_rtc_time(epoch_seconds + (int64_t)delta_seconds, result);
}

static void app_runtime_log_sleep_transition(const app_bootstrap_context_t *context,
                                             const char *event_text,
                                             uint32_t seconds_until_wake)
{
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

    if (context == NULL)
    {
        return;
    }

    ESP_LOGI(SLEEP_TAG,
             "%s mode=%d rtc=%04u-%02u-%02u %02u:%02u:%02u wake_in=%us wake_cause=%d wifi=%s",
             event_text,
             (int)context->runtime_mode,
             context->rtc_time.year,
             context->rtc_time.month,
             context->rtc_time.day,
             context->rtc_time.hour,
             context->rtc_time.minute,
             context->rtc_time.second,
             (unsigned)seconds_until_wake,
             (int)wakeup_cause,
             wifi_platform_status_text());
}

static bool app_runtime_prepare_deep_sleep(const app_bootstrap_context_t *context,
                                           uint32_t *seconds_until_wake)
{
    const board_config_t *board = board_get_config();
    uint32_t transition_seconds;
    bool next_is_sleep = false;

    if (context == NULL || seconds_until_wake == NULL || board == NULL || !context->rtc_time.valid)
    {
        return false;
    }

    if (!app_sleep_schedule_seconds_until_transition(&context->config,
                                                     &context->rtc_time,
                                                     &transition_seconds,
                                                     &next_is_sleep))
    {
        return false;
    }

    if (next_is_sleep)
    {
        return false;
    }

    ESP_ERROR_CHECK(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL));
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup((uint64_t)transition_seconds * 1000000ULL));
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << board->buttons.user_gpio,
                                                    board->buttons.active_low ? ESP_EXT1_WAKEUP_ANY_LOW
                                                                              : ESP_EXT1_WAKEUP_ANY_HIGH));

    *seconds_until_wake = transition_seconds;
    return true;
}

static void app_runtime_enter_scheduled_deep_sleep(app_bootstrap_context_t *context)
{
    uint32_t seconds_until_wake = 0;

    if (context == NULL)
    {
        return;
    }

    if (!app_runtime_prepare_deep_sleep(context, &seconds_until_wake))
    {
        ESP_LOGW(SLEEP_TAG, "deep sleep preparation failed; staying in scheduled sleep loop");
        return;
    }

    app_runtime_log_sleep_transition(context, "entering deep sleep", seconds_until_wake);
    esp_deep_sleep_start();
}

static bool app_runtime_refresh_rtc(app_bootstrap_context_t *context)
{
    if (context == NULL)
    {
        return false;
    }

    if (!sensor_service_read_rtc(&context->rtc_time))
    {
        return false;
    }

    ui_boot_model_set_rtc(&context->ui_boot_model, &context->rtc_time);
    return true;
}

static void app_runtime_update_ui_network_status(app_bootstrap_context_t *context)
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

static bool app_runtime_fetch_and_push_image(app_bootstrap_context_t *context, bool *ui_needs_update)
{
    http_request_t request;
    uint8_t *image_buffer = NULL;
    http_response_meta_t meta = {0};
    bool ok = false;

    if (context == NULL || ui_needs_update == NULL)
    {
        return false;
    }

    if (!display_port_init(context->ui_boot_model.board))
    {
        ESP_LOGE(TAG, "display port init failed during image fetch");
        return false;
    }

    image_buffer = heap_caps_malloc(IMAGE_FETCH_BUFFER_BYTES, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (image_buffer == NULL)
    {
        ESP_LOGE(TAG, "failed to allocate image buffer size=%u", (unsigned)IMAGE_FETCH_BUFFER_BYTES);
        return false;
    }

    http_request_init(&request);
    if (!http_request_set_url(&request, context->config.display.image_url))
    {
        free(image_buffer);
        return false;
    }
    request.timeout_ms = IMAGE_FETCH_TIMEOUT_MS;

    if (!http_client_get_binary(&request, image_buffer, IMAGE_FETCH_BUFFER_BYTES, &meta) ||
        meta.status != HTTP_CLIENT_STATUS_OK)
    {
        ESP_LOGW(TAG,
                 "image fetch failed url=%s status=%d code=%u",
                 context->config.display.image_url,
                 (int)meta.status,
                 (unsigned)meta.http_status_code);
        free(image_buffer);
        return false;
    }

    if (meta.bytes_received == 0)
    {
        ESP_LOGW(TAG, "image fetch returned zero bytes");
        free(image_buffer);
        return false;
    }

    /* Mark every overlay widget dirty BEFORE writing the bitmap. The BMP write
       overwrites the whole framebuffer, so anything that LVGL has not already
       scheduled for redraw would be skipped on the next lv_refr_now() and the
       battery icon would never reappear on top of the new image. */
    ui_image_overlay_invalidate();

    if (!display_port_render_bmp(image_buffer, meta.bytes_received))
    {
        ESP_LOGE(TAG, "display_port_render_bmp failed bytes=%u", (unsigned)meta.bytes_received);
        free(image_buffer);
        return false;
    }

    free(image_buffer);
    ok = true;

    int64_t wall_epoch = 0;
    if (app_rtc_time_to_epoch(&context->rtc_time, &wall_epoch))
    {
        ui_boot_model_set_image_loaded(&context->ui_boot_model, wall_epoch);
    }
    *ui_needs_update = true;
    ui_image_overlay_mark_stale(false);
    return ok;
}

static uint32_t app_runtime_compute_wait_seconds(const app_bootstrap_context_t *context, int64_t now_monotonic_seconds)
{
    uint32_t wait_seconds;
    uint32_t transition_seconds;

    if (context == NULL)
    {
        return 1;
    }

    if (context->runtime_mode == APP_RUNTIME_MODE_SCHEDULED_SLEEP)
    {
        if (context->rtc_time.valid &&
            app_sleep_schedule_seconds_until_transition(&context->config,
                                                        &context->rtc_time,
                                                        &transition_seconds,
                                                        NULL))
        {
            return transition_seconds > 0 ? transition_seconds : 1;
        }

        return FALLBACK_SLEEP_WAIT_SECONDS;
    }

    if (context->runtime_mode == APP_RUNTIME_MODE_WAKING)
    {
        return 1;
    }

    wait_seconds = app_scheduler_next_wake_delay_seconds(&context->scheduler_state,
                                                         now_monotonic_seconds,
                                                         &context->config,
                                                         time_service_get_state());

    if (context->rtc_time.valid &&
        app_sleep_schedule_seconds_until_transition(&context->config,
                                                    &context->rtc_time,
                                                    &transition_seconds,
                                                    NULL) &&
        transition_seconds < wait_seconds)
    {
        wait_seconds = transition_seconds;
    }

    if (context->runtime_mode == APP_RUNTIME_MODE_MANUAL_ACTIVE_OVERRIDE)
    {
        if (context->manual_override_until_monotonic == 0)
        {
            return wait_seconds > 0 ? wait_seconds : 1;
        }

        if (context->manual_override_until_monotonic <= now_monotonic_seconds)
        {
            return 1;
        }

        transition_seconds = (uint32_t)(context->manual_override_until_monotonic - now_monotonic_seconds);
        if (transition_seconds < wait_seconds)
        {
            wait_seconds = transition_seconds;
        }
    }

    return wait_seconds > 0 ? wait_seconds : 1;
}

static void app_runtime_wait_until_next_due(app_bootstrap_context_t *context)
{
    TickType_t wait_ticks;
    uint32_t wait_seconds;

    if (context == NULL)
    {
        return;
    }

    wait_seconds = app_runtime_compute_wait_seconds(context, esp_timer_get_time() / 1000000);
    wait_ticks = pdMS_TO_TICKS(wait_seconds * 1000U);
    if (wait_ticks == 0)
    {
        wait_ticks = 1;
    }

    (void)ulTaskNotifyTake(pdTRUE, wait_ticks);
}

static void app_runtime_apply_schedule_state(app_bootstrap_context_t *context,
                                             int64_t now_monotonic_seconds,
                                             bool key_pressed,
                                             bool *force_refresh,
                                             bool *ui_needs_update)
{
    bool active_by_schedule = true;
    bool have_schedule_state;

    if (context == NULL || force_refresh == NULL || ui_needs_update == NULL)
    {
        return;
    }

    have_schedule_state = app_sleep_schedule_is_active_now(&context->config,
                                                           &context->rtc_time,
                                                           &active_by_schedule);

    if (context->runtime_mode == APP_RUNTIME_MODE_MANUAL_ACTIVE_OVERRIDE)
    {
        if (have_schedule_state && active_by_schedule)
        {
            app_runtime_set_mode(context, APP_RUNTIME_MODE_SCHEDULED_ACTIVE);
            *ui_needs_update = true;
        }
        else if (context->manual_override_until_epoch > 0)
        {
            int64_t now_wall_epoch;

            if (app_rtc_time_to_epoch(&context->rtc_time, &now_wall_epoch) &&
                now_wall_epoch >= context->manual_override_until_epoch)
            {
                app_runtime_set_mode(context,
                                     (have_schedule_state && !active_by_schedule)
                                         ? APP_RUNTIME_MODE_SCHEDULED_SLEEP
                                         : APP_RUNTIME_MODE_SCHEDULED_ACTIVE);
                *ui_needs_update = true;
            }
        }
        else if (context->manual_override_until_monotonic > 0 &&
                 now_monotonic_seconds >= context->manual_override_until_monotonic)
        {
            app_runtime_set_mode(context,
                                 (have_schedule_state && !active_by_schedule)
                                     ? APP_RUNTIME_MODE_SCHEDULED_SLEEP
                                     : APP_RUNTIME_MODE_SCHEDULED_ACTIVE);
            *ui_needs_update = true;
        }
    }

    if (context->runtime_mode == APP_RUNTIME_MODE_SCHEDULED_SLEEP)
    {
        if (have_schedule_state && active_by_schedule)
        {
            app_runtime_set_mode(context, APP_RUNTIME_MODE_SCHEDULED_ACTIVE);
            *force_refresh = true;
            *ui_needs_update = true;
        }
    }
    else if (context->runtime_mode == APP_RUNTIME_MODE_SCHEDULED_ACTIVE)
    {
        if (have_schedule_state && !active_by_schedule)
        {
            app_runtime_set_mode(context, APP_RUNTIME_MODE_SCHEDULED_SLEEP);
            *ui_needs_update = true;
        }
    }

    if (!key_pressed)
    {
        return;
    }

    if (context->runtime_mode == APP_RUNTIME_MODE_SCHEDULED_SLEEP)
    {
        app_runtime_set_mode(context, APP_RUNTIME_MODE_WAKING);
        if (app_runtime_manual_override_seconds(context) > 0)
        {
            uint32_t override_seconds = app_runtime_manual_override_seconds(context);
            int64_t now_wall_epoch;

            context->manual_override_until_monotonic = now_monotonic_seconds + override_seconds;
            if (app_rtc_time_to_epoch(&context->rtc_time, &now_wall_epoch))
            {
                context->manual_override_until_epoch = now_wall_epoch + (int64_t)override_seconds;
            }
        }
        *force_refresh = true;
        *ui_needs_update = true;
        return;
    }

    if (context->runtime_mode == APP_RUNTIME_MODE_MANUAL_ACTIVE_OVERRIDE)
    {
        if (app_runtime_manual_override_seconds(context) > 0)
        {
            uint32_t override_seconds = app_runtime_manual_override_seconds(context);
            int64_t now_wall_epoch;

            context->manual_override_until_monotonic = now_monotonic_seconds + override_seconds;
            if (app_rtc_time_to_epoch(&context->rtc_time, &now_wall_epoch))
            {
                context->manual_override_until_epoch = now_wall_epoch + (int64_t)override_seconds;
            }
        }
    }

    *force_refresh = true;
}

static void app_runtime_apply_post_work_schedule(app_bootstrap_context_t *context, bool *ui_needs_update)
{
    bool active_by_schedule = true;

    if (context == NULL || ui_needs_update == NULL || !context->rtc_time.valid)
    {
        return;
    }

    if (!app_sleep_schedule_is_active_now(&context->config, &context->rtc_time, &active_by_schedule))
    {
        return;
    }

    if (context->runtime_mode == APP_RUNTIME_MODE_SCHEDULED_ACTIVE && !active_by_schedule)
    {
        app_runtime_set_mode(context, APP_RUNTIME_MODE_SCHEDULED_SLEEP);
        *ui_needs_update = true;
        return;
    }

    if (context->runtime_mode == APP_RUNTIME_MODE_MANUAL_ACTIVE_OVERRIDE && active_by_schedule)
    {
        app_runtime_set_mode(context, APP_RUNTIME_MODE_SCHEDULED_ACTIVE);
        *ui_needs_update = true;
    }
}

static void app_runtime_run_active_cycle(app_bootstrap_context_t *context,
                                         int64_t now_monotonic_seconds,
                                         bool force_refresh,
                                         bool *ui_needs_update)
{
    app_scheduler_due_t due;
    bool network_work_due;
    bool network_ready = false;
    bool image_attempted = false;
    bool image_ok = false;
    bool duty_cycle_mode;

    if (context == NULL || ui_needs_update == NULL)
    {
        return;
    }

    duty_cycle_mode = app_config_image_refresh_uses_wifi_duty_cycle(&context->config);

    app_scheduler_compute_due(&context->scheduler_state,
                              now_monotonic_seconds,
                              &context->config,
                              time_service_get_state(),
                              &due);

    if (force_refresh)
    {
        due.clock_due = true;
        due.environment_due = true;
        due.power_due = true;
        due.image_due = true;
    }

    network_work_due = due.image_due || due.ntp_due;
    if (network_work_due && context->config.wifi.enabled)
    {
        network_ready = wifi_platform_ensure_ready(&context->config, WIFI_RUNTIME_WAIT_TIMEOUT_MS);
        app_runtime_update_ui_network_status(context);
        *ui_needs_update = true;
    }

    if (due.clock_due)
    {
        if (app_runtime_refresh_rtc(context))
        {
            app_scheduler_note_clock_rendered(&context->scheduler_state, now_monotonic_seconds);
            *ui_needs_update = true;
        }
    }

    if (due.environment_due)
    {
        if (sensor_service_read_environment(&context->environment))
        {
            ui_boot_model_set_environment(&context->ui_boot_model, &context->environment);
            app_scheduler_note_environment_polled(&context->scheduler_state, now_monotonic_seconds);
            *ui_needs_update = true;
        }
    }

    if (due.power_due)
    {
        if (power_service_get_status(&context->power_status))
        {
            ui_boot_model_set_power(&context->ui_boot_model, &context->power_status);
            *ui_needs_update = true;
        }

        app_scheduler_note_power_polled(&context->scheduler_state, now_monotonic_seconds);
    }

    if (due.image_due)
    {
        image_attempted = true;

        if (context->config.wifi.enabled && !network_ready)
        {
            ESP_LOGW(TAG, "image refresh skipped: %s", wifi_platform_status_text());
            image_ok = false;
        }
        else
        {
            image_ok = app_runtime_fetch_and_push_image(context, ui_needs_update);
        }

        app_scheduler_note_image_polled(&context->scheduler_state, now_monotonic_seconds);
    }

    if (image_attempted && app_runtime_refresh_rtc(context))
    {
        *ui_needs_update = true;
    }

    if (due.ntp_due)
    {
        int64_t synced_epoch;

        ESP_LOGI(TAG, "ntp sync due");
        if (network_ready && time_service_sync_ntp(NTP_RUNTIME_WAIT_TIMEOUT_MS, now_monotonic_seconds, &synced_epoch) &&
            app_epoch_to_rtc_time(synced_epoch, &context->rtc_time))
        {
            (void)sensor_service_write_rtc(&context->rtc_time);
            ui_boot_model_set_rtc(&context->ui_boot_model, &context->rtc_time);
            app_scheduler_note_clock_rendered(&context->scheduler_state, now_monotonic_seconds);
            *ui_needs_update = true;
        }
        app_scheduler_note_ntp_attempted(&context->scheduler_state, now_monotonic_seconds);
    }

    if (network_work_due && context->config.wifi.enabled && duty_cycle_mode)
    {
        (void)wifi_platform_stop();
        app_runtime_update_ui_network_status(context);
        *ui_needs_update = true;
    }
}

static void app_runtime_task(void *arg)
{
    app_bootstrap_context_t *context = (app_bootstrap_context_t *)arg;

    app_key_input_register_task(xTaskGetCurrentTaskHandle());

    while (true)
    {
        app_runtime_step(context, esp_timer_get_time() / 1000000);
        app_runtime_wait_until_next_due(context);
    }
}

void app_runtime_step(app_bootstrap_context_t *context, int64_t now_epoch_seconds)
{
    bool ui_needs_update = false;
    bool force_refresh = false;
    bool key_pressed;

    if (context == NULL)
    {
        return;
    }

    (void)app_runtime_refresh_rtc(context);

    key_pressed = app_key_input_consume_press();
    app_runtime_apply_schedule_state(context,
                                     now_epoch_seconds,
                                     key_pressed,
                                     &force_refresh,
                                     &ui_needs_update);

    if (context->runtime_mode == APP_RUNTIME_MODE_WAKING)
    {
        if (ui_needs_update)
        {
            ui_app_update(&context->ui_boot_model);
            ui_needs_update = false;
        }

        app_runtime_set_mode(context, APP_RUNTIME_MODE_MANUAL_ACTIVE_OVERRIDE);
        force_refresh = true;

        if (app_runtime_manual_override_seconds(context) > 0 &&
            context->manual_override_until_monotonic == 0)
        {
            uint32_t override_seconds = app_runtime_manual_override_seconds(context);
            int64_t now_wall_epoch;

            context->manual_override_until_monotonic = now_epoch_seconds + override_seconds;
            if (app_rtc_time_to_epoch(&context->rtc_time, &now_wall_epoch))
            {
                context->manual_override_until_epoch = now_wall_epoch + (int64_t)override_seconds;
            }
        }
    }

    if (context->runtime_mode == APP_RUNTIME_MODE_SCHEDULED_SLEEP)
    {
        if (ui_needs_update)
        {
            ui_app_update(&context->ui_boot_model);
        }
        app_runtime_enter_scheduled_deep_sleep(context);
        return;
    }

    app_runtime_run_active_cycle(context,
                                 now_epoch_seconds,
                                 force_refresh,
                                 &ui_needs_update);

    (void)app_runtime_refresh_rtc(context);
    app_runtime_apply_post_work_schedule(context, &ui_needs_update);

    if (ui_needs_update)
    {
        ui_app_update(&context->ui_boot_model);
    }
}

bool app_runtime_start(app_bootstrap_context_t *context)
{
    BaseType_t result;

    if (context == NULL)
    {
        return false;
    }

    result = xTaskCreate(app_runtime_task, "app_runtime", APP_RUNTIME_TASK_STACK_SIZE, context, 5, NULL);
    return result == pdPASS;
}
