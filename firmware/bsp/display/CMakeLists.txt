idf_component_register(SRCS "bmp_to_rlcd.c" "display_port.c" "rlcd_driver.c"
					INCLUDE_DIRS "."
					REQUIRES esp_driver_gpio esp_driver_spi esp_lcd esp_timer heap log lvgl waveshare-s3-rlcd-4.2)