#include "sensor_manager.h"
#include "communication_manager.h"
#include "gpio_pinout.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "SENSOR_MGR";
static adc_oneshot_unit_handle_t adc_handle;
static TaskHandle_t sensor_task_handle;

// Sensor data structure
typedef struct {
    float fuel_level;
    float onboard_temperature;
    float cabin_temperature;
    float humidity;
    uint16_t co2_level;
    uint16_t light_level;
    bool van_light_active;
    bool door_open;
} sensor_data_t;

static sensor_data_t current_sensors;

esp_err_t sensor_manager_init(void) {
    esp_err_t ret;
    
    // Initialize ADC for analog sensors
    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    
    ret = adc_oneshot_new_unit(&adc_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure ADC channels
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    
    // Fuel gauge ADC channel
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_2, &chan_config); // FUEL_GAUGE
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure fuel gauge ADC channel");
        return ret;
    }
    
    // Onboard temperature ADC channel
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_1, &chan_config); // TEMP_ONBOARD
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure temperature ADC channel");
        return ret;
    }
    
    // Configure UART for HCO2T sensor
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT
    };
    
    ret = uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver");
        return ret;
    }
    
    ret = uart_param_config(UART_NUM_1, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART");
        return ret;
    }
    
    ret = uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, HCO2T_TX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins");
        return ret;
    }
    
    // Configure van light input pin
    gpio_config_t van_light_config = {
        .pin_bit_mask = (1ULL << VAN_LIGHT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&van_light_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure van light input pin");
        return ret;
    }
    
    // Initialize sensor data
    memset(&current_sensors, 0, sizeof(sensor_data_t));
    
    // Create sensor task
    BaseType_t result = xTaskCreate(
        sensor_manager_task,
        "sensor_manager",
        4096,
        NULL,
        3,
        &sensor_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Sensor manager initialized");
    return ESP_OK;
}

static float read_fuel_level(void) {
    int adc_value;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_2, &adc_value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read fuel level ADC");
        return current_sensors.fuel_level; // Return last known value
    }
    
    // Convert ADC value to fuel percentage (0-100%)
    // Assuming linear relationship, adjust based on actual sensor characteristics
    float fuel_percentage = (adc_value / 4095.0f) * 100.0f;
    return fuel_percentage;
}

static float read_onboard_temperature(void) {
    int adc_value;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_1, &adc_value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read onboard temperature ADC");
        return current_sensors.onboard_temperature;
    }
    
    // Convert ADC value to temperature (assuming NTC thermistor)
    // This is a simplified conversion, adjust based on actual sensor
    float voltage = (adc_value / 4095.0f) * 3.3f;
    float resistance = (voltage * 10000.0f) / (3.3f - voltage);
    float temperature = (1.0f / (log(resistance / 10000.0f) / 3950.0f + 1.0f / 298.15f)) - 273.15f;
    
    return temperature;
}

static void parse_hco2t_data(const char *data, size_t len) {
    // Parse comma-separated values: humidity,co2,temperature,light
    char *token;
    char *data_copy = malloc(len + 1);
    if (data_copy == NULL) return;
    
    memcpy(data_copy, data, len);
    data_copy[len] = '\0';
    
    int field = 0;
    token = strtok(data_copy, ",");
    while (token != NULL && field < 4) {
        float value = atof(token);
        switch (field) {
            case 0: current_sensors.humidity = value; break;
            case 1: current_sensors.co2_level = (uint16_t)value; break;
            case 2: current_sensors.cabin_temperature = value; break;
            case 3: current_sensors.light_level = (uint16_t)value; break;
        }
        token = strtok(NULL, ",");
        field++;
    }
    
    free(data_copy);
}

void sensor_manager_task(void *parameters) {
    ESP_LOGI(TAG, "Sensor manager task started");
    
    while (1) {
        // Read analog sensors
        current_sensors.fuel_level = read_fuel_level();
        current_sensors.onboard_temperature = read_onboard_temperature();
        
        // Read van light state
        current_sensors.van_light_active = gpio_get_level(VAN_LIGHT);
        
        // Read HCO2T sensor data via UART
        uint8_t uart_data[128];
        int len = uart_read_bytes(UART_NUM_1, uart_data, sizeof(uart_data) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            uart_data[len] = '\0';
            parse_hco2t_data((char*)uart_data, len);
        }
        
        // Determine door state based on van light and ambient light
        static bool prev_van_light = false;
        if (current_sensors.van_light_active && !prev_van_light) {
            // Van light just turned on, check ambient light
            if (current_sensors.light_level < DOOR_OPEN_LIGHT_THRESHOLD) {
                current_sensors.door_open = true;
                ESP_LOGI(TAG, "Door opened detected");
            }
        } else if (!current_sensors.van_light_active && prev_van_light) {
            current_sensors.door_open = false;
            ESP_LOGI(TAG, "Door closed detected");
        }
        prev_van_light = current_sensors.van_light_active;
        
        // Send sensor data to communication manager
        comm_send_message(COMM_MSG_SENSOR_UPDATE, &current_sensors, sizeof(sensor_data_t));
        
        // Check for critical conditions
        if (current_sensors.fuel_level < FUEL_EMPTY_THRESHOLD) {
            uint32_t error = ERROR_HEATER_NO_FUEL;
            comm_send_message(COMM_MSG_ERROR, &error, sizeof(uint32_t));
        }
        
        vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_INTERVAL_MS));
    }
}

float sensor_get_fuel_level(void) {
    return current_sensors.fuel_level;
}

bool sensor_is_door_open(void) {
    return current_sensors.door_open;
}
