#include "app_config.h"

#include <string.h>

static bool string_has_value(const char *value)
{
    return value != NULL && value[0] != '\0';
}

static uint16_t normalized_sleep_schedule_minutes(uint16_t minutes)
{
    return minutes == (24U * 60U) ? 0U : minutes;
}

void app_config_init(app_config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->version = 3;
    config->source = CONFIG_SOURCE_NONE;
    config->device.ntp_enabled = true;
    config->device.ntp_sync_hours = 24;
    strncpy(config->device.timezone, "Asia/Shanghai", sizeof(config->device.timezone) - 1);
    config->device.sleep_schedule.enabled = false;
    config->device.sleep_schedule.wake_minutes = APP_CONFIG_TIME_UNSET;
    config->device.sleep_schedule.sleep_minutes = APP_CONFIG_TIME_UNSET;
    config->device.sleep_schedule.manual_wake_minutes = 5;
    config->wifi.enabled = false;

    strncpy(config->display.image_url,
            APP_CONFIG_DEFAULT_IMAGE_URL,
            sizeof(config->display.image_url) - 1);
    config->display.image_refresh_seconds = APP_CONFIG_DEFAULT_IMAGE_REFRESH_SECONDS;
}

bool app_config_image_refresh_uses_wifi_duty_cycle(const app_config_t *config)
{
    if (config == NULL)
    {
        return false;
    }
    return config->display.image_refresh_seconds >= APP_CONFIG_WIFI_DUTY_CYCLE_THRESHOLD_SECONDS;
}

bool app_config_validate(const app_config_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    if (config->version == 0)
    {
        return false;
    }

    if (config->wifi.enabled && !string_has_value(config->wifi.ssid))
    {
        return false;
    }

    if (config->device.sleep_schedule.enabled)
    {
        const sleep_schedule_config_t *schedule = &config->device.sleep_schedule;

        if (schedule->wake_minutes == APP_CONFIG_TIME_UNSET ||
            schedule->sleep_minutes == APP_CONFIG_TIME_UNSET)
        {
            return false;
        }

        if (schedule->wake_minutes >= (24U * 60U) ||
            schedule->sleep_minutes > (24U * 60U))
        {
            return false;
        }

        if (normalized_sleep_schedule_minutes(schedule->wake_minutes) ==
            normalized_sleep_schedule_minutes(schedule->sleep_minutes))
        {
            return false;
        }
    }

    if (config->device.sleep_schedule.manual_wake_minutes > (24U * 60U))
    {
        return false;
    }

    if (!string_has_value(config->display.image_url))
    {
        return false;
    }

    if (config->display.image_refresh_seconds < APP_CONFIG_MIN_IMAGE_REFRESH_SECONDS ||
        config->display.image_refresh_seconds > APP_CONFIG_MAX_IMAGE_REFRESH_SECONDS)
    {
        return false;
    }

    return true;
}
