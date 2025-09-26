#include "w5500_ethernet.h"
#include "gpio_pinout.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "W5500_ETH";
static spi_device_handle_t spi_handle;
static esp_eth_handle_t eth_handle;
static bool slave_connected = false;

esp_err_t w5500_ethernet_init(void) {
    esp_err_t ret;
    
    // Configure SPI bus
    spi_bus_config_t bus_config = {
        .mosi_io_num = SPI_MOSI,
        .miso_io_num = SPI_MISO,
        .sclk_io_num = SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1024
    };
    
    ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure SPI device
    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = 10 * 1000 * 1000, // 10 MHz
        .mode = 0,
        .spics_io_num = SPI_CS,
        .queue_size = 7
    };
    
    ret = spi_bus_add_device(SPI2_HOST, &dev_config, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure W5500 reset pin
    gpio_config_t reset_config = {
        .pin_bit_mask = (1ULL << W5500_RST_1),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&reset_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure reset pin: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Reset W5500
    gpio_set_level(W5500_RST_1, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(W5500_RST_1, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "W5500 Ethernet initialized");
    return ESP_OK;
}

esp_err_t w5500_send_state(van_state_t *state) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Implement actual W5500 communication
    // For now, just log that we're sending state
    ESP_LOGD(TAG, "Sending state to SlavePCB");
    
    return ESP_OK;
}

esp_err_t w5500_receive_command(van_command_t *cmd) {
    if (cmd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Implement actual W5500 receive
    // For now, return no data available
    return ESP_ERR_NOT_FOUND;
}

bool w5500_is_slave_connected(void) {
    return slave_connected;
}
