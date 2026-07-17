#include "sensor_service.h"

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum
{
    RTC_ADDRESS = 0x51,
    RTC_REG_SECONDS = 0x04,
    RTC_READ_LEN = 7,
    SHTC3_ADDRESS = 0x70,
    SHTC3_CMD_READ_ID = 0xEFC8,
    SHTC3_CMD_WAKEUP = 0x3517,
    SHTC3_CMD_SOFT_RESET = 0x805D,
    SHTC3_CMD_SLEEP = 0xB219,
    SHTC3_CMD_MEASURE_T_RH = 0x7866,
    I2C_TIMEOUT_MS = 1000,
    SHTC3_TEMP_OFFSET_C = 4,
};

static const board_config_t *s_board_config;
static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_rtc_dev;
static i2c_master_dev_handle_t s_shtc3_dev;
static bool s_rtc_ready;
static bool s_shtc3_ready;

static const char *TAG = "sensor_service";

static uint8_t bcd_to_dec(uint8_t value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

static uint8_t dec_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10) << 4) | (value % 10));
}

static esp_err_t i2c_add_device(uint8_t address, uint32_t speed_hz, i2c_master_dev_handle_t *device)
{
    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = speed_hz,
    };

    return i2c_master_bus_add_device(s_i2c_bus, &device_config, device);
}

static esp_err_t shtc3_write_command(uint16_t command)
{
    uint8_t buffer[2] = {
        (uint8_t)(command >> 8),
        (uint8_t)(command & 0xFF),
    };

    return i2c_master_transmit(s_shtc3_dev, buffer, sizeof(buffer), pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static uint8_t shtc3_crc(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xFF;
    size_t index;
    uint8_t bit;

    for (index = 0; index < length; ++index)
    {
        crc ^= data[index];
        for (bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }

    return crc;
}

static float shtc3_calc_temperature(uint16_t raw_value)
{
    return (175.0f * (float)raw_value / 65536.0f) - 45.0f - (float)SHTC3_TEMP_OFFSET_C;
}

static float shtc3_calc_humidity(uint16_t raw_value)
{
    return 100.0f * (float)raw_value / 65536.0f;
}

void sensor_service_init(const board_config_t *board)
{
    esp_err_t error;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = board != NULL ? board->i2c.sda_gpio : -1,
        .scl_io_num = board != NULL ? board->i2c.scl_gpio : -1,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    uint8_t rtc_probe_reg = RTC_REG_SECONDS;
    uint8_t rtc_probe_value = 0;
    uint8_t shtc3_id_command[2] = {
        (uint8_t)(SHTC3_CMD_READ_ID >> 8),
        (uint8_t)(SHTC3_CMD_READ_ID & 0xFF),
    };
    uint8_t shtc3_id_response[3] = {0};

    s_board_config = board;
    s_i2c_bus = NULL;
    s_rtc_dev = NULL;
    s_shtc3_dev = NULL;
    s_rtc_ready = false;
    s_shtc3_ready = false;

    if (board == NULL)
    {
        return;
    }

    error = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c bus init failed err=%d", (int)error);
        return;
    }

    error = i2c_add_device(RTC_ADDRESS, 300000, &s_rtc_dev);
    if (error == ESP_OK)
    {
        error = i2c_master_transmit_receive(s_rtc_dev, &rtc_probe_reg, 1, &rtc_probe_value, 1, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        s_rtc_ready = (error == ESP_OK);
    }
    if (!s_rtc_ready)
    {
        ESP_LOGW(TAG, "rtc probe failed err=%d", (int)error);
    }

    error = i2c_add_device(SHTC3_ADDRESS, 400000, &s_shtc3_dev);
    if (error == ESP_OK)
    {
        error = shtc3_write_command(SHTC3_CMD_WAKEUP);
        if (error == ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            error = shtc3_write_command(SHTC3_CMD_SOFT_RESET);
        }
        if (error == ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            error = i2c_master_transmit_receive(s_shtc3_dev,
                                                shtc3_id_command,
                                                sizeof(shtc3_id_command),
                                                shtc3_id_response,
                                                sizeof(shtc3_id_response),
                                                pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        }
        if (error == ESP_OK && shtc3_crc(shtc3_id_response, 2) == shtc3_id_response[2])
        {
            s_shtc3_ready = true;
        }
    }
    if (!s_shtc3_ready)
    {
        ESP_LOGW(TAG, "shtc3 probe failed err=%d", (int)error);
    }
}

bool sensor_service_read_environment(environment_sample_t *sample)
{
    uint8_t bytes[6] = {0};
    uint16_t raw_temp;
    uint16_t raw_humi;
    float temp_c;
    float humidity;
    esp_err_t error;

    if (sample == NULL || s_board_config == NULL)
    {
        return false;
    }

    sample->temperature_c_x10 = 0;
    sample->humidity_rh_x10 = 0;
    sample->valid = false;

    if (!s_shtc3_ready || s_shtc3_dev == NULL)
    {
        return false;
    }

    error = shtc3_write_command(SHTC3_CMD_WAKEUP);
    if (error != ESP_OK)
    {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    error = shtc3_write_command(SHTC3_CMD_MEASURE_T_RH);
    if (error != ESP_OK)
    {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
    error = i2c_master_receive(s_shtc3_dev, bytes, sizeof(bytes), pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    /* Send the chip to sleep (typ ~0.4 uA) instead of soft-resetting it back
       to idle. The next read path already issues WAKEUP before MEASURE, so
       no extra round-trip is needed to recover from sleep. */
    (void)shtc3_write_command(SHTC3_CMD_SLEEP);
    if (error != ESP_OK)
    {
        return false;
    }

    if (shtc3_crc(bytes, 2) != bytes[2] || shtc3_crc(&bytes[3], 2) != bytes[5])
    {
        return false;
    }

    raw_temp = (uint16_t)((bytes[0] << 8) | bytes[1]);
    raw_humi = (uint16_t)((bytes[3] << 8) | bytes[4]);
    temp_c = shtc3_calc_temperature(raw_temp);
    humidity = shtc3_calc_humidity(raw_humi);
    sample->temperature_c_x10 = (int16_t)(temp_c * 10.0f);
    sample->humidity_rh_x10 = (int16_t)(humidity * 10.0f);
    sample->valid = true;

    return true;
}

bool sensor_service_read_rtc(rtc_time_t *time_value)
{
    uint8_t rtc_data[RTC_READ_LEN] = {0};
    uint8_t rtc_reg = RTC_REG_SECONDS;
    esp_err_t error;

    if (time_value == NULL || s_board_config == NULL)
    {
        return false;
    }

    time_value->year = 0;
    time_value->month = 0;
    time_value->day = 0;
    time_value->hour = 0;
    time_value->minute = 0;
    time_value->second = 0;
    time_value->valid = false;

    if (!s_rtc_ready || s_rtc_dev == NULL)
    {
        return false;
    }

    error = i2c_master_transmit_receive(s_rtc_dev, &rtc_reg, 1, rtc_data, sizeof(rtc_data), pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (error != ESP_OK)
    {
        return false;
    }

    if ((rtc_data[0] & 0x80) != 0)
    {
        return false;
    }

    time_value->second = bcd_to_dec(rtc_data[0] & 0x7F);
    time_value->minute = bcd_to_dec(rtc_data[1] & 0x7F);
    time_value->hour = bcd_to_dec(rtc_data[2] & 0x3F);
    time_value->day = bcd_to_dec(rtc_data[3] & 0x3F);
    time_value->month = bcd_to_dec(rtc_data[5] & 0x1F);
    time_value->year = (uint16_t)(2000 + bcd_to_dec(rtc_data[6]));
    time_value->valid = true;

    return true;
}

bool sensor_service_write_rtc(const rtc_time_t *time_value)
{
    uint8_t buffer[8];
    esp_err_t error;

    if (time_value == NULL || s_board_config == NULL || !time_value->valid)
    {
        return false;
    }

    if (!s_rtc_ready || s_rtc_dev == NULL)
    {
        return false;
    }

    if (time_value->year < 2000 || time_value->year > 2099)
    {
        return false;
    }

    buffer[0] = RTC_REG_SECONDS;
    buffer[1] = dec_to_bcd(time_value->second);
    buffer[2] = dec_to_bcd(time_value->minute);
    buffer[3] = dec_to_bcd(time_value->hour);
    buffer[4] = dec_to_bcd(time_value->day);
    buffer[5] = 0;
    buffer[6] = dec_to_bcd(time_value->month);
    buffer[7] = dec_to_bcd((uint8_t)(time_value->year - 2000));

    error = i2c_master_transmit(s_rtc_dev, buffer, sizeof(buffer), pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    return error == ESP_OK;
}