idf_component_register(SRCS "ui_app.c" "ui_boot_model.c" "ui_image_overlay.c" "ui_sleep_screen.c"
                    INCLUDE_DIRS "."
                    REQUIRES display domain lvgl power sensor storage waveshare-s3-rlcd-4.2)
