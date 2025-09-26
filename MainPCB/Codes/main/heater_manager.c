#include "heater_manager.h"
#include "communication_manager.h"
#include "sensor_manager.h"
#include "fan_manager.h"
#include "gpio_pinout.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "HEATER_MGR";
static TaskHandle_t heater_task_handle;

typedef struct {
    bool heater_on;
    float water_temperature;
    float target_water_temp;
    float target_cabin_temp;
    bool pump_active;
    uint8_t radiator_fan_speed;
    float pid_integral;
    float pid_last_error;
} heater_data_t;

static heater_data_t heater_data;

esp_err_t heater_manager_init(void) {
    esp_err_t ret;
    
    // Configure heater control pin
    gpio_config_t heater_config = {
        .pin_bit_mask = (1ULL << HEATER_ON_SIG),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&heater_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure heater control pin");
        return ret;
    }
    
    // Configure pump control pin
    gpio_config_t pump_config = {
        .pin_bit_mask = (1ULL << PH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&pump_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure pump control pin");
        return ret;
    }
    
    // Configure UART for heater communication
    uart_config_t uart_config = {
        .baud_rate = 25000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT
    };
    
    ret = uart_driver_install(UART_NUM_2, HEATER_UART_BUFFER_SIZE, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install heater UART driver");
        return ret;
    }
    
    ret = uart_param_config(UART_NUM_2, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure heater UART");
        return ret;
    }
    
    ret = uart_set_pin(UART_NUM_2, UART_PIN_NO_CHANGE, HEATER_TX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set heater UART pins");
        return ret;
    }
    
    // Initialize heater data
    memset(&heater_data, 0, sizeof(heater_data_t));
    heater_data.target_water_temp = 60.0f;
    heater_data.target_cabin_temp = 20.0f;
    
    // Turn off heater and pump initially
    gpio_set_level(HEATER_ON_SIG, 0);
    gpio_set_level(PH, 0);
    
    // Create heater task
    BaseType_t result = xTaskCreate(
        heater_manager_task,
        "heater_manager",
        4096,
        NULL,
        3,
        &heater_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create heater task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Heater manager initialized");
    return ESP_OK;
}

static void parse_heater_data(const char *data, size_t len) {
    // Parse heater response data
    // Expected format: "TEMP:XX.X,STATE:X"
    char *temp_ptr = strstr(data, "TEMP:");
    if (temp_ptr) {
        heater_data.water_temperature = atof(temp_ptr + 5);
    }
}

static float pid_control(float setpoint, float measured_value, float dt) {
    float error = setpoint - measured_value;
    
    // Proportional term
    float p_term = HEATER_PID_KP * error;
    
    // Integral term
    heater_data.pid_integral += error * dt;
    // Prevent integral windup
    if (heater_data.pid_integral > 100.0f) heater_data.pid_integral = 100.0f;
    if (heater_data.pid_integral < -100.0f) heater_data.pid_integral = -100.0f;
    float i_term = HEATER_PID_KI * heater_data.pid_integral;
    
    // Derivative term
    float d_term = HEATER_PID_KD * (error - heater_data.pid_last_error) / dt;
    heater_data.pid_last_error = error;
    
    float output = p_term + i_term + d_term;
    
    // Clamp output to 0-100%
    if (output > 100.0f) output = 100.0f;
    if (output < 0.0f) output = 0.0f;
    
    return output;
}

void heater_manager_task(void *parameters) {
    ESP_LOGI(TAG, "Heater manager task started");
    
    uint8_t uart_data[HEATER_UART_BUFFER_SIZE];
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        // Read heater data via UART
        int len = uart_read_bytes(UART_NUM_2, uart_data, sizeof(uart_data) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            uart_data[len] = '\0';
            parse_heater_data((char*)uart_data, len);
        }
        
        // Get current system state
        van_state_t *state = protocol_get_state();
        if (state) {
            // Check fuel level before allowing heater operation
            float fuel_level = sensor_get_fuel_level();
            if (fuel_level < 5.0f && heater_data.heater_on) {
                ESP_LOGW(TAG, "Low fuel level, turning off heater");
                heater_data.heater_on = false;
                heater_data.pump_active = false;
                gpio_set_level(HEATER_ON_SIG, 0);
                gpio_set_level(PH, 0);
                
                uint32_t error = ERROR_HEATER_NO_FUEL;
                comm_send_message(COMM_MSG_ERROR, &error, sizeof(uint32_t));
            }
            
            // PID control for water temperature
            if (heater_data.heater_on && fuel_level >= 5.0f) {
                float dt = (float)HEATER_UPDATE_INTERVAL_MS / 1000.0f;
                float pid_output = pid_control(heater_data.target_water_temp, heater_data.water_temperature, dt);
                
                // Control heater based on PID output
                if (pid_output > 10.0f) {
                    gpio_set_level(HEATER_ON_SIG, 1);
                    heater_data.pump_active = true;
                    gpio_set_level(PH, 1);
                } else {
                    gpio_set_level(HEATER_ON_SIG, 0);
                }
                
                // Control radiator fan based on water temperature and cabin temperature
                if (heater_data.water_temperature > HEATER_MIN_WATER_TEMP_FOR_FAN) {
                    float cabin_temp_error = heater_data.target_cabin_temp - state->sensors.cabin_temperature;
                    if (cabin_temp_error > 0) {
                        // Calculate fan speed based on temperature difference
                        uint8_t fan_speed = (uint8_t)(cabin_temp_error * 25.0f); // 25% per degree difference
                        if (fan_speed > 100) fan_speed = 100;
                        if (fan_speed < 20) fan_speed = 20; // Minimum speed
                        
                        heater_data.radiator_fan_speed = fan_speed;
                        fan_set_speed(FAN_HEATER, fan_speed);
                    } else {
                        heater_data.radiator_fan_speed = 0;
                        fan_set_speed(FAN_HEATER, 0);
                    }
                } else {
                    heater_data.radiator_fan_speed = 0;
                    fan_set_speed(FAN_HEATER, 0);
                }
            } else {
                // Heater is off
                gpio_set_level(HEATER_ON_SIG, 0);
                if (heater_data.water_temperature < 30.0f) {
                    // Turn off pump when water is cool
                    heater_data.pump_active = false;
                    gpio_set_level(PH, 0);
                }
                heater_data.radiator_fan_speed = 0;
                fan_set_speed(FAN_HEATER, 0);
            }
        }
        
        // Send heater data to communication manager
        comm_send_message(COMM_MSG_HEATER_UPDATE, &heater_data, sizeof(heater_data_t));
        
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(HEATER_UPDATE_INTERVAL_MS));
    }
}

esp_err_t heater_set_target_temperature(float water_temp, float cabin_temp) {
    heater_data.target_water_temp = water_temp;
    heater_data.target_cabin_temp = cabin_temp;
    ESP_LOGI(TAG, "Target temperatures set: Water=%.1f°C, Cabin=%.1f°C", water_temp, cabin_temp);
    return ESP_OK;
}

esp_err_t heater_set_state(bool enabled) {
    heater_data.heater_on = enabled;
    ESP_LOGI(TAG, "Heater %s", enabled ? "enabled" : "disabled");
    return ESP_OK;
}

float heater_get_water_temperature(void) {
    return heater_data.water_temperature;
}
