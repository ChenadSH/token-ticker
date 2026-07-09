idf_component_register(SRCS "app_bootstrap.c" "app_key_input.c" "app_scheduler.c" "app_runtime.c" "app_sleep_schedule.c"
                    INCLUDE_DIRS "."
                    REQUIRES display domain esp_driver_gpio esp_timer freertos http power sensor storage time ui waveshare-s3-rlcd-4.2 wifi)
