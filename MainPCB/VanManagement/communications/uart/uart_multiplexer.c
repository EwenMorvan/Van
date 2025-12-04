#include "uart_multiplexer.h"


static const char *TAG = "UART_MUX";

// Mutexes to protect UART access
static SemaphoreHandle_t uart0_mutex = NULL;
static SemaphoreHandle_t uart1_mutex = NULL;
static SemaphoreHandle_t uart2_mutex = NULL;


// Current device connected to each UART
static mppt_device_t current_mppt_device = MPPT_100_50;
static sensor_device_t current_sensor_device = SENSOR_HEATER;

// UART configurations
static uart_config_t com_uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
};

static uart_config_t mppt_uart_config = {
    .baud_rate = 19200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
    .source_clk = UART_SCLK_DEFAULT
};

static uart_config_t sensor_uart_config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
    .source_clk = UART_SCLK_DEFAULT
};

esp_err_t uart_multiplexer_init(void) {
    esp_err_t ret;
    
    // Create mutexes
    uart0_mutex = xSemaphoreCreateMutex();
    uart1_mutex = xSemaphoreCreateMutex();
    uart2_mutex = xSemaphoreCreateMutex();

    if (uart0_mutex == NULL || uart1_mutex == NULL || uart2_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutexes");
        return ESP_FAIL;
    }
    // Install UART0 driver for general communication
    ret = uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART0 driver");
        return ret;
    }
    
    // Install UART1 driver for MPPT devices
    ret = uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART1 driver");
        return ret;
    }
    
    // Install UART2 driver for sensor devices
    ret = uart_driver_install(UART_NUM_2, 1024, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART2 driver");
        return ret;
    }
    
    // Configure UART0 for general communication
    ret = uart_param_config(UART_NUM_0, &com_uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART0");
        return ret;
    }
    ret = uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART0 pins");
        return ret;
    }

    // Configure UART1 for MPPT (start with MPPT 100|50)
    ret = uart_param_config(UART_NUM_1, &mppt_uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART1");
        return ret;
    }
    
    ret = uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, VE_DIRECT_RX0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART1 pins");
        return ret;
    }
    
    // Configure UART2 for sensors (start with heater)
    ret = uart_param_config(UART_NUM_2, &sensor_uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART2");
        return ret;
    }
    
    ret = uart_set_pin(UART_NUM_2, UART_PIN_NO_CHANGE, HEATER_TX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART2 pins");
        return ret;
    }
    
    ESP_LOGI(TAG, "UART multiplexer initialized");
    return ESP_OK;
}

esp_err_t uart_mux_switch_mppt(mppt_device_t device) {
    if (xSemaphoreTake(uart1_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take UART1 mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    if (current_mppt_device == device) {
        xSemaphoreGive(uart1_mutex);
        return ESP_OK; // Already connected to this device
    }
    
    esp_err_t ret = ESP_OK;
    
    // Flush any pending data
    uart_flush(UART_NUM_1);
    
    // Switch pins based on device
    switch (device) {
        case MPPT_100_50:
            ESP_LOGI(TAG, "Switching UART1 to MPPT 100|50 (RX pin %d)", VE_DIRECT_RX0);
            ret = uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, VE_DIRECT_RX0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
            break;
        case MPPT_70_15:
            ESP_LOGI(TAG, "Switching UART1 to MPPT 70|15 (RX pin %d)", VE_DIRECT_RX1);
            ret = uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, VE_DIRECT_RX1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
            break;
        default:
            ret = ESP_ERR_INVALID_ARG;
            break;
    }
    
    if (ret == ESP_OK) {
        current_mppt_device = device;
        ESP_LOGD(TAG, "Successfully switched UART1 to MPPT device %d", device);
        // Small delay to ensure pin switching is complete
        vTaskDelay(pdMS_TO_TICKS(10));
    } else {
        ESP_LOGE(TAG, "Failed to switch UART1 to MPPT device %d: %s", device, esp_err_to_name(ret));
    }
    
    xSemaphoreGive(uart1_mutex);
    return ret;
}

esp_err_t uart_mux_switch_sensor(sensor_device_t device) {
    if (xSemaphoreTake(uart2_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take UART2 mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    if (current_sensor_device == device) {
        xSemaphoreGive(uart2_mutex);
        return ESP_OK; // Already connected to this device
    }
    
    esp_err_t ret = ESP_OK;
    
    // Flush any pending data
    uart_flush(UART_NUM_2);
    
    // Switch pins based on device  
    switch (device) {
        case SENSOR_HEATER:
            ESP_LOGI(TAG, "Switching UART2 to Heater (RX pin %d)", HEATER_TX);
            ret = uart_set_pin(UART_NUM_2, UART_PIN_NO_CHANGE, HEATER_TX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
            break;
        case SENSOR_HCO2T:
            ESP_LOGI(TAG, "Switching UART2 to HCO2T sensor (RX pin %d)", HCO2T_TX);
            ret = uart_set_pin(UART_NUM_2, UART_PIN_NO_CHANGE, HCO2T_TX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
            break;
        default:
            ret = ESP_ERR_INVALID_ARG;
            break;
    }
    
    if (ret == ESP_OK) {
        current_sensor_device = device;
        ESP_LOGD(TAG, "Successfully switched UART2 to sensor device %d", device);
        // Small delay to ensure pin switching is complete
        vTaskDelay(pdMS_TO_TICKS(10));
    } else {
        ESP_LOGE(TAG, "Failed to switch UART2 to sensor device %d: %s", device, esp_err_to_name(ret));
    }
    
    xSemaphoreGive(uart2_mutex);
    return ret;
}

// void uart_com_reader_task(void *pvParameters) {
//     uint8_t data[128];
//     while (1) {
//         int len = uart_read_bytes(UART_NUM_0, data, sizeof(data)-1, pdMS_TO_TICKS(100));
//         if (len > 0) {
//             data[len] = 0;
//             ESP_LOGI(TAG, "UART received: %s", (char*)data);
//         }
//     }
// }
bool uart_com_reader() {
    uint8_t data[128];
    int len = uart_read_bytes(UART_NUM_0, data, sizeof(data)-1, pdMS_TO_TICKS(50));
    if (len > 0) {
        data[len] = 0;
        // ESP_LOGI(TAG, "UART received: %s", (char*)data);
        return true;
    }
    return false;
}

int uart_mux_read_mppt(uint8_t *data, size_t max_len, uint32_t timeout_ms) {
    if (xSemaphoreTake(uart1_mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take UART1 mutex for read");
        return -1;
    }
    
    ESP_LOGD(TAG, "Reading from UART1 (current device: %d), timeout: %lu ms", current_mppt_device, timeout_ms);
    int len = uart_read_bytes(UART_NUM_1, data, max_len, pdMS_TO_TICKS(timeout_ms));
    ESP_LOGD(TAG, "UART1 read returned %d bytes", len);
    
    xSemaphoreGive(uart1_mutex);
    return len;
}

int uart_mux_read_sensor(uint8_t *data, size_t max_len, uint32_t timeout_ms) {
    if (xSemaphoreTake(uart2_mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take UART2 mutex for read");
        return -1;
    }
    
    ESP_LOGD(TAG, "Reading from UART2 (current device: %d), timeout: %lu ms", current_sensor_device, timeout_ms);
    int len = uart_read_bytes(UART_NUM_2, data, max_len, pdMS_TO_TICKS(timeout_ms));
    ESP_LOGD(TAG, "UART2 read returned %d bytes", len);
    
    xSemaphoreGive(uart2_mutex);
    return len;
}
