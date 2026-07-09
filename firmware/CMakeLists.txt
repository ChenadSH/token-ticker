cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    app
    bsp/boards/waveshare-s3-rlcd-4.2
    bsp/display
    bsp/power
    bsp/sensor
    domain
    platform/http
    platform/storage
    platform/telemetry
    platform/time
    platform/wifi
    ui
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(token_ticker)
