#ifndef TOKEN_TICKER_APP_SCHEDULER_H
#define TOKEN_TICKER_APP_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "time_service.h"

typedef struct
{
    bool clock_due;
    bool image_due;
    bool environment_due;
    bool power_due;
    bool ntp_due;
} app_scheduler_due_t;

typedef struct
{
    int64_t last_clock_minute;
    int64_t last_image_poll_epoch;
    int64_t last_environment_poll_epoch;
    int64_t last_power_poll_epoch;
    int64_t last_ntp_attempt_epoch;
} app_scheduler_state_t;

void app_scheduler_init(app_scheduler_state_t *state);
void app_scheduler_compute_due(const app_scheduler_state_t *state,
                               int64_t now_epoch_seconds,
                               const app_config_t *config,
                               const time_service_state_t *time_state,
                               app_scheduler_due_t *due);
uint32_t app_scheduler_next_wake_delay_seconds(const app_scheduler_state_t *state,
                                               int64_t now_epoch_seconds,
                                               const app_config_t *config,
                                               const time_service_state_t *time_state);
void app_scheduler_note_clock_rendered(app_scheduler_state_t *state, int64_t now_epoch_seconds);
void app_scheduler_note_image_polled(app_scheduler_state_t *state, int64_t now_epoch_seconds);
void app_scheduler_note_environment_polled(app_scheduler_state_t *state, int64_t now_epoch_seconds);
void app_scheduler_note_power_polled(app_scheduler_state_t *state, int64_t now_epoch_seconds);
void app_scheduler_note_ntp_attempted(app_scheduler_state_t *state, int64_t now_epoch_seconds);

#endif
