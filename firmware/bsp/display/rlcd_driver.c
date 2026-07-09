#include "rlcd_driver.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "rlcd_driver";

static void rlcd_driver_configure_sleep_compatibility(const board_config_t *board)
{
    const int gpio_pins[] = {
        board->display.cs_gpio,
        board->display.sck_gpio,
        board->display.mosi_gpio,
        board->display.dc_gpio,
        board->display.rst_gpio,
    };
    size_t index;

    for (index = 0; index < (sizeof(gpio_pins) / sizeof(gpio_pins[0])); ++index)
    {
        esp_err_t error = gpio_sleep_sel_dis((gpio_num_t)gpio_pins[index]);
        if (error != ESP_OK)
        {
            ESP_LOGW(TAG, "gpio_sleep_sel_dis failed gpio=%d err=%d", gpio_pins[index], (int)error);
        }
    }

    ESP_LOGI(TAG, "display gpio sleep bypass configured");
}

static void rlcd_reset(const board_config_t *board)
{
    gpio_set_level((gpio_num_t)board->display.rst_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level((gpio_num_t)board->display.rst_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level((gpio_num_t)board->display.rst_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void rlcd_send_command(rlcd_driver_t *driver, uint8_t command)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(driver->io_handle, command, NULL, 0));
}

static void rlcd_send_data(rlcd_driver_t *driver, uint8_t data)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(driver->io_handle, -1, &data, 1));
}

static void rlcd_send_buffer(rlcd_driver_t *driver, const uint8_t *data, size_t len)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(driver->io_handle, -1, data, len));
}

static void rlcd_init_sequence(rlcd_driver_t *driver, const board_config_t *board)
{
    rlcd_reset(board);

    rlcd_send_command(driver, 0xD6);
    rlcd_send_data(driver, 0x17);
    rlcd_send_data(driver, 0x02);
    rlcd_send_command(driver, 0xD1);
    rlcd_send_data(driver, 0x01);
    rlcd_send_command(driver, 0xC0);
    rlcd_send_data(driver, 0x11);
    rlcd_send_data(driver, 0x04);
    rlcd_send_command(driver, 0xC1);
    rlcd_send_data(driver, 0x69);
    rlcd_send_data(driver, 0x69);
    rlcd_send_data(driver, 0x69);
    rlcd_send_data(driver, 0x69);
    rlcd_send_command(driver, 0xC2);
    rlcd_send_data(driver, 0x19);
    rlcd_send_data(driver, 0x19);
    rlcd_send_data(driver, 0x19);
    rlcd_send_data(driver, 0x19);
    rlcd_send_command(driver, 0xC4);
    rlcd_send_data(driver, 0x4B);
    rlcd_send_data(driver, 0x4B);
    rlcd_send_data(driver, 0x4B);
    rlcd_send_data(driver, 0x4B);
    rlcd_send_command(driver, 0xC5);
    rlcd_send_data(driver, 0x19);
    rlcd_send_data(driver, 0x19);
    rlcd_send_data(driver, 0x19);
    rlcd_send_data(driver, 0x19);
    rlcd_send_command(driver, 0xD8);
    rlcd_send_data(driver, 0x80);
    rlcd_send_data(driver, 0xE9);
    rlcd_send_command(driver, 0xB2);
    rlcd_send_data(driver, 0x02);
    rlcd_send_command(driver, 0xB3);
    rlcd_send_data(driver, 0xE5);
    rlcd_send_data(driver, 0xF6);
    rlcd_send_data(driver, 0x05);
    rlcd_send_data(driver, 0x46);
    rlcd_send_data(driver, 0x77);
    rlcd_send_data(driver, 0x77);
    rlcd_send_data(driver, 0x77);
    rlcd_send_data(driver, 0x77);
    rlcd_send_data(driver, 0x76);
    rlcd_send_data(driver, 0x45);
    rlcd_send_command(driver, 0xB4);
    rlcd_send_data(driver, 0x05);
    rlcd_send_data(driver, 0x46);
    rlcd_send_data(driver, 0x77);
    rlcd_send_data(driver, 0x77);
    rlcd_send_data(driver, 0x77);
    rlcd_send_data(driver, 0x77);
    rlcd_send_data(driver, 0x76);
    rlcd_send_data(driver, 0x45);
    rlcd_send_command(driver, 0x62);
    rlcd_send_data(driver, 0x32);
    rlcd_send_data(driver, 0x03);
    rlcd_send_data(driver, 0x1F);
    rlcd_send_command(driver, 0xB7);
    rlcd_send_data(driver, 0x13);
    rlcd_send_command(driver, 0xB0);
    rlcd_send_data(driver, 0x64);
    rlcd_send_command(driver, 0x11);
    vTaskDelay(pdMS_TO_TICKS(200));
    rlcd_send_command(driver, 0xC9);
    rlcd_send_data(driver, 0x00);
    rlcd_send_command(driver, 0x36);
    rlcd_send_data(driver, 0x48);
    rlcd_send_command(driver, 0x3A);
    rlcd_send_data(driver, 0x11);
    rlcd_send_command(driver, 0xB9);
    rlcd_send_data(driver, 0x20);
    rlcd_send_command(driver, 0xB8);
    rlcd_send_data(driver, 0x29);
    rlcd_send_command(driver, 0x21);
    rlcd_send_command(driver, 0x2A);
    rlcd_send_data(driver, 0x12);
    rlcd_send_data(driver, 0x2A);
    rlcd_send_command(driver, 0x2B);
    rlcd_send_data(driver, 0x00);
    rlcd_send_data(driver, 0xC7);
    rlcd_send_command(driver, 0x35);
    rlcd_send_data(driver, 0x00);
    rlcd_send_command(driver, 0xD0);
    rlcd_send_data(driver, 0xFF);
    rlcd_send_command(driver, 0x38);
    rlcd_send_command(driver, 0x29);
}

static void rlcd_enter_sleep_sequence(rlcd_driver_t *driver)
{
    rlcd_send_command(driver, 0x28);
    vTaskDelay(pdMS_TO_TICKS(20));
    rlcd_send_command(driver, 0x10);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void rlcd_exit_sleep_sequence(rlcd_driver_t *driver)
{
    rlcd_send_command(driver, 0x11);
    vTaskDelay(pdMS_TO_TICKS(200));
    rlcd_send_command(driver, 0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
}

static bool rlcd_compute_index(const rlcd_driver_t *driver, int x, int y, size_t *index, uint8_t *mask)
{
    if (driver == NULL || index == NULL || mask == NULL)
    {
        return false;
    }

    if (x < 0 || x >= driver->width || y < 0 || y >= driver->height)
    {
        return false;
    }

    if (driver->landscape)
    {
        const int inv_y = driver->height - 1 - y;
        const int block_y = inv_y >> 2;
        const int local_y = inv_y & 3;
        const int byte_x = x >> 1;
        const int local_x = x & 1;
        const int bytes_per_column = driver->height >> 2;
        *index = (size_t)(byte_x * bytes_per_column + block_y);
        *mask = (uint8_t)(1U << (7 - ((local_y << 1) | local_x)));
        return true;
    }

    {
        const int bytes_per_row = driver->width >> 2;
        const int byte_y = y >> 1;
        const int local_y = y & 1;
        const int byte_x = x >> 2;
        const int local_x = x & 3;
        *index = (size_t)(byte_y * bytes_per_row + byte_x);
        *mask = (uint8_t)(1U << (7 - ((local_x << 1) | local_y)));
        return true;
    }
}

bool rlcd_driver_init(rlcd_driver_t *driver, const board_config_t *board)
{
    esp_err_t ret;
    gpio_config_t rst_gpio = {0};
    spi_bus_config_t buscfg = {0};
    esp_lcd_panel_io_spi_config_t io_config = {0};
    const size_t transfer_pixels = (size_t)board->display.display_width * (size_t)board->display.display_height;

    if (driver == NULL || board == NULL)
    {
        return false;
    }

    memset(driver, 0, sizeof(*driver));
    driver->width = board->display.display_width;
    driver->height = board->display.display_height;
    driver->landscape = board->display.landscape;
    driver->framebuffer_len = transfer_pixels >> 3;

    driver->framebuffer = heap_caps_malloc(driver->framebuffer_len, MALLOC_CAP_8BIT);
    if (driver->framebuffer == NULL)
    {
        ESP_LOGE(TAG, "framebuffer allocation failed");
        return false;
    }

    buscfg.miso_io_num = -1;
    buscfg.mosi_io_num = board->display.mosi_gpio;
    buscfg.sclk_io_num = board->display.sck_gpio;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = (int)transfer_pixels;
    ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "spi_bus_initialize failed err=%d", (int)ret);
        return false;
    }

    io_config.dc_gpio_num = board->display.dc_gpio;
    io_config.cs_gpio_num = board->display.cs_gpio;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 7;
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &driver->io_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed err=%d", (int)ret);
        return false;
    }

    rst_gpio.intr_type = GPIO_INTR_DISABLE;
    rst_gpio.mode = GPIO_MODE_OUTPUT;
    rst_gpio.pin_bit_mask = (1ULL << board->display.rst_gpio);
    rst_gpio.pull_down_en = GPIO_PULLDOWN_DISABLE;
    rst_gpio.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&rst_gpio));

    rlcd_driver_configure_sleep_compatibility(board);

    rlcd_init_sequence(driver, board);
    driver->initialized = true;
    driver->sleeping = false;

    /* Push an unmistakable startup pattern so on-device debugging can distinguish
       a dead transport from higher-level UI issues. */
    rlcd_driver_clear(driver, false);
    rlcd_driver_display(driver);
    vTaskDelay(pdMS_TO_TICKS(120));
    rlcd_driver_clear(driver, true);
    rlcd_driver_display(driver);
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_LOGI(TAG, "startup test pattern pushed");
    return true;
}

void rlcd_driver_deinit(rlcd_driver_t *driver)
{
    if (driver == NULL)
    {
        return;
    }

    if (driver->framebuffer != NULL)
    {
        free(driver->framebuffer);
        driver->framebuffer = NULL;
    }
    driver->sleeping = false;
    driver->initialized = false;
}

void rlcd_driver_clear(rlcd_driver_t *driver, bool white)
{
    if (driver == NULL || driver->framebuffer == NULL)
    {
        return;
    }

    memset(driver->framebuffer, white ? 0xFF : 0x00, driver->framebuffer_len);
}

void rlcd_driver_set_pixel(rlcd_driver_t *driver, int x, int y, bool white)
{
    size_t index;
    uint8_t mask;

    if (driver == NULL || driver->framebuffer == NULL)
    {
        return;
    }

    if (!rlcd_compute_index(driver, x, y, &index, &mask) || index >= driver->framebuffer_len)
    {
        return;
    }

    if (white)
    {
        driver->framebuffer[index] |= mask;
    }
    else
    {
        driver->framebuffer[index] &= (uint8_t)~mask;
    }
}

void rlcd_driver_display(rlcd_driver_t *driver)
{
    if (driver == NULL || !driver->initialized || driver->framebuffer == NULL || driver->sleeping)
    {
        return;
    }

    rlcd_send_command(driver, 0x2A);
    rlcd_send_data(driver, 0x12);
    rlcd_send_data(driver, 0x2A);
    rlcd_send_command(driver, 0x2B);
    rlcd_send_data(driver, 0x00);
    rlcd_send_data(driver, 0xC7);
    rlcd_send_command(driver, 0x2C);
    rlcd_send_buffer(driver, driver->framebuffer, driver->framebuffer_len);
}

bool rlcd_driver_write_bitmap(rlcd_driver_t *driver,
                              const uint8_t *bitmap,
                              size_t bitmap_len)
{
    if (driver == NULL || driver->framebuffer == NULL || bitmap == NULL)
    {
        return false;
    }

    if (bitmap_len != driver->framebuffer_len)
    {
        ESP_LOGW(TAG, "bitmap size mismatch got=%u expected=%u",
                 (unsigned)bitmap_len,
                 (unsigned)driver->framebuffer_len);
        return false;
    }

    memcpy(driver->framebuffer, bitmap, bitmap_len);
    return true;
}

bool rlcd_driver_set_sleep(rlcd_driver_t *driver, bool sleep)
{
    if (driver == NULL || !driver->initialized)
    {
        return false;
    }

    if (sleep)
    {
        if (driver->sleeping)
        {
            return true;
        }

        rlcd_enter_sleep_sequence(driver);
        driver->sleeping = true;
        ESP_LOGI(TAG, "panel entered sleep");
        return true;
    }

    if (!driver->sleeping)
    {
        return true;
    }

    rlcd_exit_sleep_sequence(driver);
    driver->sleeping = false;
    ESP_LOGI(TAG, "panel exited sleep");
    return true;
}