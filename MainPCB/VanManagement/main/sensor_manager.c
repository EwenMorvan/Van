#include "sensor_manager.h"
#include "communication_manager.h"
#include "uart_multiplexer.h"
#include "gpio_pinout.h"
#include "protocol.h"  // For ENABLE_SIMULATION
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
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
        ESP_LOGE(TAG, "Failed to initialize ADC unit");
        return ret;
    }
    
    // Configure ADC channels
    adc_oneshot_chan_cfg_t adc_chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_0
    };
    
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_8, &adc_chan_config); // TEMP_ONBOARD
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel for temperature");
        return ret;
    }
    
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_5, &adc_chan_config); // FUEL_GAUGE
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel for fuel");
        return ret;
    }
    
    // Configure GPIO inputs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << INTER) | (1ULL << VAN_LIGHT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO inputs");
        return ret;
    }
    
    // Initialize sensor data
    memset(&current_sensors, 0, sizeof(sensor_data_t));
    
    // Create sensor task on CPU1 for better load balancing
    BaseType_t result = xTaskCreatePinnedToCore(
        sensor_manager_task,
        "sensor_manager",
        4096,
        NULL,
        4,
        &sensor_task_handle,
        1  // CPU1
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Sensor manager initialized");
    return ESP_OK;
}

static float read_fuel_level(void) {
    int adc_reading;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_5, &adc_reading);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read fuel level ADC");
        return -1.0f;
    }
    
    // Convert ADC reading to fuel percentage (0-100%)
    // Assuming linear relationship, adjust based on actual sensor characteristics
    float voltage = (float)adc_reading / 4095.0f * 3.3f;
    float fuel_percentage = (voltage / 3.3f) * 100.0f;
    
    return fuel_percentage;
}

static float read_onboard_temperature(void) {
    int adc_reading;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_8, &adc_reading);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read temperature ADC");
        return -999.0f;
    }
    
    // Convert ADC reading to temperature in Celsius
    // Assuming thermistor or temperature sensor with linear output
    float voltage = (float)adc_reading / 4095.0f * 3.3f;
    float temperature = (voltage - 0.5f) * 100.0f; // Example conversion for TMP36
    
    return temperature;
}

static void parse_hco2t_data(const char *data, int len) {
    // Parse data from HT/CO2/Light sensor
    // Expected format: "T:25.3,H:65.2,C:450,L:1234\n"
    char *token;
    char *data_copy = malloc(len + 1);
    if (!data_copy) return;
    
    strncpy(data_copy, data, len);
    data_copy[len] = '\0';
    
    token = strtok(data_copy, ",");
    while (token != NULL) {
        if (strncmp(token, "T:", 2) == 0) {
            current_sensors.cabin_temperature = atof(token + 2);
        } else if (strncmp(token, "H:", 2) == 0) {
            current_sensors.humidity = atof(token + 2);
        } else if (strncmp(token, "C:", 2) == 0) {
            current_sensors.co2_level = (uint16_t)atoi(token + 2);
        } else if (strncmp(token, "L:", 2) == 0) {
            current_sensors.light_level = (uint16_t)atoi(token + 2);
        }
        token = strtok(NULL, ",");
    }
    
    free(data_copy);
}

static void parse_heater_data(const char *data, int len) {
    // Parse data from heater
    // Implementation depends on heater protocol
    ESP_LOGD(TAG, "Heater data: %.*s", len, data);
}

void sensor_manager_task(void *parameters) {
    ESP_LOGI(TAG, "Sensor manager task started");
    
    bool read_heater_cycle = false; // Alternate between HCO2T sensor and heater reading
    
    while (1) {
        // Read analog sensors (always)
        current_sensors.fuel_level = read_fuel_level();
        current_sensors.onboard_temperature = read_onboard_temperature();
        
        // Read van light state (always)
        current_sensors.van_light_active = gpio_get_level(VAN_LIGHT);
        
        // Alternate between HCO2T sensor and heater reading
        if (read_heater_cycle) {
            // Read from heater
            ESP_LOGD(TAG, "Reading from heater (UART2 -> GPIO %d)", HEATER_TX);
            if (uart_mux_switch_sensor(SENSOR_HEATER) == ESP_OK) {
                uint8_t uart_data[128];
                int len = uart_mux_read_sensor(uart_data, sizeof(uart_data) - 1, 500);
                ESP_LOGD(TAG, "Heater: Read %d bytes", len);
                if (len > 0) {
                    uart_data[len] = '\0';
                    ESP_LOG_BUFFER_HEXDUMP(TAG, uart_data, len, ESP_LOG_DEBUG);
                    parse_heater_data((char*)uart_data, len);
                }
            }
        } else {
            // Read from HCO2T sensor
            ESP_LOGD(TAG, "Reading from HCO2T sensor (UART2 -> GPIO %d)", HCO2T_TX);
            if (uart_mux_switch_sensor(SENSOR_HCO2T) == ESP_OK) {
                uint8_t uart_data[128];
                int len = uart_mux_read_sensor(uart_data, sizeof(uart_data) - 1, 500);
                ESP_LOGD(TAG, "HCO2T: Read %d bytes", len);
                if (len > 0) {
                    uart_data[len] = '\0';
                    ESP_LOG_BUFFER_HEXDUMP(TAG, uart_data, len, ESP_LOG_DEBUG);
                    parse_hco2t_data((char*)uart_data, len);
                }
            }
        }
        
        // Toggle reading mode for next cycle
        read_heater_cycle = !read_heater_cycle;
        
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
#if !ENABLE_SIMULATION  // Only check real sensor errors when simulation is disabled
        if (current_sensors.fuel_level < FUEL_EMPTY_THRESHOLD) {
            uint32_t error = ERROR_HEATER_NO_FUEL;
            comm_send_message(COMM_MSG_ERROR, &error, sizeof(uint32_t));
        }
#endif
        
        ESP_LOGD(TAG, "Sensors: Fuel=%.1f%%, OnbT=%.1f°C, CabT=%.1f°C, H=%.1f%%, CO2=%dppm, Light=%d", 
                current_sensors.fuel_level, current_sensors.onboard_temperature, 
                current_sensors.cabin_temperature, current_sensors.humidity, 
                current_sensors.co2_level, current_sensors.light_level);
        
        vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_INTERVAL_MS));
    }
}

float sensor_get_fuel_level(void) {
    return current_sensors.fuel_level;
}

bool sensor_is_door_open(void) {
    return current_sensors.door_open;
}
