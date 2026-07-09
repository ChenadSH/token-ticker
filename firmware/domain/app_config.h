#ifndef TOKEN_TICKER_APP_CONFIG_H
#define TOKEN_TICKER_APP_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_CONFIG_ID_LEN 32
#define APP_CONFIG_SECRET_LEN 128
#define APP_CONFIG_HOST_LEN 192
#define APP_CONFIG_TZ_LEN 48
#define APP_CONFIG_WIFI_SSID_LEN 33
#define APP_CONFIG_WIFI_PASSWORD_LEN 65
#define APP_CONFIG_IMAGE_URL_LEN 192
#define APP_CONFIG_TIME_UNSET UINT16_MAX
#define APP_CONFIG_DEFAULT_IMAGE_REFRESH_SECONDS 300
#define APP_CONFIG_DEFAULT_IMAGE_URL "http://124.221.91.97:39080/render"
#define APP_CONFIG_MIN_IMAGE_REFRESH_SECONDS 30
#define APP_CONFIG_MAX_IMAGE_REFRESH_SECONDS 86400
#define APP_CONFIG_WIFI_DUTY_CYCLE_THRESHOLD_SECONDS 120

typedef enum
{
    CONFIG_SOURCE_NONE = 0,
    CONFIG_SOURCE_SD_CARD,
    CONFIG_SOURCE_NVS,
    CONFIG_SOURCE_SERIAL,
} config_source_t;

typedef struct
{
    bool enabled;
    uint16_t wake_minutes;
    uint16_t sleep_minutes;
    uint16_t manual_wake_minutes;
} sleep_schedule_config_t;

typedef struct
{
    bool ntp_enabled;
    uint16_t ntp_sync_hours;
    char timezone[APP_CONFIG_TZ_LEN];
    sleep_schedule_config_t sleep_schedule;
} device_config_t;

typedef struct
{
    bool enabled;
    char ssid[APP_CONFIG_WIFI_SSID_LEN];
    char password[APP_CONFIG_WIFI_PASSWORD_LEN];
} app_wifi_config_t;

typedef struct
{
    char image_url[APP_CONFIG_IMAGE_URL_LEN];
    uint32_t image_refresh_seconds;
} display_config_t;

typedef struct
{
    uint32_t version;
    config_source_t source;
    device_config_t device;
    app_wifi_config_t wifi;
    display_config_t display;
} app_config_t;

void app_config_init(app_config_t *config);
bool app_config_validate(const app_config_t *config);
bool app_config_image_refresh_uses_wifi_duty_cycle(const app_config_t *config);

#endif
