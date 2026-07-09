#include "app_scheduler.h"

#include <limits.h>
#include <string.h>

enum
{
    ENVIRONMENT_REFRESH_INTERVAL_SECONDS = 15 * 60,
    POWER_REFRESH_INTERVAL_SECONDS = 30 * 60,
    NTP_RETRY_INTERVAL_SECONDS = 30 * 60,
};

void app_scheduler_init(app_scheduler_state_t *state)
{
    if (state == NULL)
    {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->last_clock_minute = -1;
}

static uint32_t app_scheduler_image_interval_seconds(const app_scheduler_state_t *state,
                                                     const app_config_t *config)
{
    (void)state;
    if (config == NULL || config->display.image_refresh_seconds == 0)
    {
        return APP_CONFIG_DEFAULT_IMAGE_REFRESH_SECONDS;
    }
    return config->display.image_refresh_seconds;
}

void app_scheduler_compute_due(const app_scheduler_state_t *state,
                               int64_t now_epoch_seconds,
                               const app_config_t *config,
                               const time_service_state_t *time_state,
                               app_scheduler_due_t *due)
{
    int64_t current_minute;
    uint32_t image_interval;

    if (state == NULL || due == NULL)
    {
        return;
    }

    memset(due, 0, sizeof(*due));

    if (now_epoch_seconds < 0)
    {
        return;
    }

    current_minute = now_epoch_seconds / 60;
    due->clock_due = current_minute != state->last_clock_minute;

    image_interval = app_scheduler_image_interval_seconds(state, config);
    due->image_due = (state->last_image_poll_epoch == 0) ||
                     ((now_epoch_seconds - state->last_image_poll_epoch) >= (int64_t)image_interval);

    due->environment_due = (state->last_environment_poll_epoch == 0) ||
                            ((now_epoch_seconds - state->last_environment_poll_epoch) >= ENVIRONMENT_REFRESH_INTERVAL_SECONDS);

    due->power_due = (state->last_power_poll_epoch == 0) ||
                      ((now_epoch_seconds - state->last_power_poll_epoch) >= POWER_REFRESH_INTERVAL_SECONDS);

    due->ntp_due = time_service_should_sync_ntp(now_epoch_seconds) &&
                    ((state->last_ntp_attempt_epoch == 0) ||
                     ((now_epoch_seconds - state->last_ntp_attempt_epoch) >= NTP_RETRY_INTERVAL_SECONDS));
}

uint32_t app_scheduler_next_wake_delay_seconds(const app_scheduler_state_t *state,
                                               int64_t now_epoch_seconds,
                                               const app_config_t *config,
                                               const time_service_state_t *time_state)
{
    app_scheduler_due_t due;
    int64_t next_due_epoch = INT64_MAX;
    uint32_t image_interval;
    int64_t ntp_interval_seconds;
    int64_t ntp_sync_epoch;
    int64_t ntp_retry_epoch;

    if (state == NULL || now_epoch_seconds < 0)
    {
        return 1;
    }

    app_scheduler_compute_due(state, now_epoch_seconds, config, time_state, &due);

    if (due.clock_due || due.image_due || due.environment_due || due.power_due || due.ntp_due)
    {
        return 1;
    }

    if (state->last_clock_minute >= 0)
    {
        next_due_epoch = ((state->last_clock_minute + 1) * 60);
    }

    image_interval = app_scheduler_image_interval_seconds(state, config);
    if (state->last_image_poll_epoch > 0)
    {
        const int64_t image_due_epoch = state->last_image_poll_epoch + (int64_t)image_interval;
        if (image_due_epoch < next_due_epoch)
        {
            next_due_epoch = image_due_epoch;
        }
    }

    if (state->last_environment_poll_epoch > 0)
    {
        const int64_t environment_due_epoch = state->last_environment_poll_epoch + ENVIRONMENT_REFRESH_INTERVAL_SECONDS;
        if (environment_due_epoch < next_due_epoch)
        {
            next_due_epoch = environment_due_epoch;
        }
    }

    if (state->last_power_poll_epoch > 0)
    {
        const int64_t power_due_epoch = state->last_power_poll_epoch + POWER_REFRESH_INTERVAL_SECONDS;
        if (power_due_epoch < next_due_epoch)
        {
            next_due_epoch = power_due_epoch;
        }
    }

    if (time_state != NULL && time_state->ntp_enabled)
    {
        ntp_interval_seconds = (int64_t)time_state->ntp_sync_hours * 3600;
        ntp_sync_epoch = (time_state->last_ntp_sync_monotonic_epoch <= 0)
                             ? now_epoch_seconds
                             : time_state->last_ntp_sync_monotonic_epoch + ntp_interval_seconds;
        ntp_retry_epoch = (state->last_ntp_attempt_epoch <= 0)
                              ? now_epoch_seconds
                              : state->last_ntp_attempt_epoch + NTP_RETRY_INTERVAL_SECONDS;

        if (ntp_sync_epoch < ntp_retry_epoch)
        {
            ntp_sync_epoch = ntp_retry_epoch;
        }

        if (ntp_sync_epoch < next_due_epoch)
        {
            next_due_epoch = ntp_sync_epoch;
        }
    }

    if (next_due_epoch == INT64_MAX || next_due_epoch <= now_epoch_seconds)
    {
        return 1;
    }

    return (uint32_t)(next_due_epoch - now_epoch_seconds);
}

void app_scheduler_note_clock_rendered(app_scheduler_state_t *state, int64_t now_epoch_seconds)
{
    if (state == NULL || now_epoch_seconds < 0)
    {
        return;
    }

    state->last_clock_minute = now_epoch_seconds / 60;
}

void app_scheduler_note_image_polled(app_scheduler_state_t *state, int64_t now_epoch_seconds)
{
    if (state == NULL || now_epoch_seconds < 0)
    {
        return;
    }

    state->last_image_poll_epoch = now_epoch_seconds;
}

void app_scheduler_note_environment_polled(app_scheduler_state_t *state, int64_t now_epoch_seconds)
{
    if (state == NULL || now_epoch_seconds < 0)
    {
        return;
    }

    state->last_environment_poll_epoch = now_epoch_seconds;
}

void app_scheduler_note_power_polled(app_scheduler_state_t *state, int64_t now_epoch_seconds)
{
    if (state == NULL || now_epoch_seconds < 0)
    {
        return;
    }

    state->last_power_poll_epoch = now_epoch_seconds;
}

void app_scheduler_note_ntp_attempted(app_scheduler_state_t *state, int64_t now_epoch_seconds)
{
    if (state == NULL || now_epoch_seconds < 0)
    {
        return;
    }

    state->last_ntp_attempt_epoch = now_epoch_seconds;
}
