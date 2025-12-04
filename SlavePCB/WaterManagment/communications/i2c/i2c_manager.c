#include "i2c_manager.h"

static const char *TAG = "I2C_MGR";

// I2C configuration
#define I2C_MASTER_SCL_IO    I2C_MUX_SCL
#define I2C_MASTER_SDA_IO    I2C_MUX_SDA
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MULTIPLEXER_ADDR 0x70

slave_pcb_err_t i2c_manager_init(void) {
    ESP_LOGI(TAG, "Initializing I2C on SDA=%d, SCL=%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_I2C_FAIL, TAG, "I2C param config failed", ret);
        return SLAVE_PCB_ERR_I2C_FAIL;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        // ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_I2C_FAIL, TAG, "I2C driver install failed", ret);
        return SLAVE_PCB_ERR_I2C_FAIL;
    }

    ESP_LOGI(TAG, "I2C initialized successfully");
    return SLAVE_PCB_OK;
}

slave_pcb_err_t i2c_set_multiplexer_channel(uint8_t channel) {
    if (channel > 7) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Try I2C-based multiplexer control first
    uint8_t channel_byte = 1 << channel;
    esp_err_t i2c_ret = i2c_master_write_to_device(I2C_MASTER_NUM, I2C_MULTIPLEXER_ADDR, 
                                                   &channel_byte, 1, pdMS_TO_TICKS(100));
    
    if (i2c_ret == ESP_OK) {
        ESP_LOGD(TAG, "I2C multiplexer channel %d selected", channel);
        vTaskDelay(pdMS_TO_TICKS(5));
        return SLAVE_PCB_OK;
    }
    
    // GPIO fallback
    ESP_LOGW(TAG, "I2C multiplexer control failed, using GPIO fallback");
    REPORT_ERROR(SLAVE_PCB_ERR_I2C_FAIL, TAG, "I2C multiplexer control failed", 0);
    gpio_set_level(I2C_MUX_A0, (channel & 0x01) ? 1 : 0);
    gpio_set_level(I2C_MUX_A1, (channel & 0x02) ? 1 : 0);
    gpio_set_level(I2C_MUX_A2, (channel & 0x04) ? 1 : 0);

    vTaskDelay(pdMS_TO_TICKS(5));
    return SLAVE_PCB_OK;
}

slave_pcb_err_t i2c_write_register(uint8_t device_addr, uint8_t reg, uint16_t value) {
    uint8_t data[3] = {reg, (value >> 8) & 0xFF, value & 0xFF};
    
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, device_addr, data, 3, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C write register 0x%02X failed: %s", reg, esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_I2C_FAIL, TAG, "I2C write register failed", ret);
        return SLAVE_PCB_ERR_I2C_FAIL;
    }
    
    return SLAVE_PCB_OK;
}

slave_pcb_err_t i2c_read_register(uint8_t device_addr, uint8_t reg, uint16_t *value) {
    if (!value) return SLAVE_PCB_ERR_INVALID_ARG;

    // Write register address
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, device_addr, &reg, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C write register address failed: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_I2C_FAIL, TAG, "I2C write register address failed", ret);
        return SLAVE_PCB_ERR_I2C_FAIL;
    }
    
    // Read register value
    uint8_t data[2];
    ret = i2c_master_read_from_device(I2C_MASTER_NUM, device_addr, data, 2, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C read register data failed: %s", esp_err_to_name(ret));
        REPORT_ERROR(SLAVE_PCB_ERR_I2C_FAIL, TAG, "I2C read register data failed", ret);
        return SLAVE_PCB_ERR_I2C_FAIL;
    }
    
    *value = (data[0] << 8) | data[1];
    return SLAVE_PCB_OK;
}