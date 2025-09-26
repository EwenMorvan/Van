#include "heater_manager.h"
#include "communication_manager.h"
#include "sensor_manager.h"
#include "fan_manager.h"
#include "gpio_pinout.h"
#include "protocol.h"  // For ENABLE_SIMULATION
#include "esp_log.h"
#include "driver/gpio.h"
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
    
    // Configure heater control outputs
    gpio_config_t output_conf = {
        .pin_bit_mask = (1ULL << HEATER_ON_SIG) | (1ULL << PH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&output_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure heater outputs");
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
    
    ESP_LOGI(TAG, "Heater manager initialized (UART handled by sensor_manager)");
    return ESP_OK;
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
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    // Wait a bit for other managers to initialize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while (1) {
        // Note: Heater UART data is now read by sensor_manager via multiplexer
        // We get the water temperature from the communication manager's van state
        
        // Get current system state
        van_state_t *state = protocol_get_state();
        if (state) {
            // Update our water temperature from heater data (received via sensor_manager)
            // This would come from the communication manager's aggregated data
            // For now, we simulate or get it from sensor readings
            
            // Check fuel level before allowing heater operation
            float fuel_level = sensor_get_fuel_level();
#if !ENABLE_SIMULATION  // Only check real fuel level when simulation is disabled
            if (fuel_level < 5.0f && heater_data.heater_on) {
                ESP_LOGW(TAG, "Low fuel level, turning off heater");
                heater_data.heater_on = false;
                heater_data.pump_active = false;
                gpio_set_level(HEATER_ON_SIG, 0);
                gpio_set_level(PH, 0);
                
                uint32_t error = ERROR_HEATER_NO_FUEL;
                comm_send_message(COMM_MSG_ERROR, &error, sizeof(uint32_t));
            }
#endif
            
            // PID control for water temperature
#if ENABLE_SIMULATION
            // In simulation mode, use simulated fuel level
            van_state_t *state = protocol_get_state();
            float effective_fuel_level = state ? state->sensors.fuel_level : 50.0f;
            if (heater_data.heater_on && effective_fuel_level >= 5.0f) {
#else
            if (heater_data.heater_on && fuel_level >= 5.0f) {
#endif
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
                        esp_err_t ret = fan_set_speed(FAN_HEATER, fan_speed);
                        if (ret != ESP_OK) {
                            ESP_LOGW(TAG, "Failed to set fan speed");
                        }
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
        esp_err_t ret = comm_send_message(COMM_MSG_HEATER_UPDATE, &heater_data, sizeof(heater_data_t));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send heater update message");
        }
        
        ESP_LOGD(TAG, "Heater: %s, Water=%.1f°C, Target=%.1f°C, Pump=%s, Fan=%d%%", 
                heater_data.heater_on ? "ON" : "OFF",
                heater_data.water_temperature,
                heater_data.target_water_temp,
                heater_data.pump_active ? "ON" : "OFF",
                heater_data.radiator_fan_speed);
        
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

esp_err_t heater_update_water_temperature(float temperature) {
    heater_data.water_temperature = temperature;
    return ESP_OK;
}
