#include "protocol.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

#include "log_level.h"
#include "fan_manager.h"

static const char *TAG = "PROTOCOL";
static van_state_t van_state;
static SemaphoreHandle_t state_mutex;
static led_mode_t led_modes_roof[MAX_LED_MODES];
static led_mode_t led_modes_exterior[MAX_LED_MODES];

void protocol_init(void) {
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return;
    }
    
    // Initialize state with default values
    memset(&van_state, 0, sizeof(van_state_t));
    memset(led_modes_roof, 0, sizeof(led_modes_roof));
    memset(led_modes_exterior, 0, sizeof(led_modes_exterior));
    
    // Set default LED mode (natural white)
    for (int i = 0; i < MAX_LED_COUNT; i++) {
        led_modes_roof[0].white[i] = 255;
        led_modes_exterior[0].white[i] = 255;
    }
    led_modes_roof[0].brightness = 255;
    led_modes_roof[0].enabled = true;
    led_modes_exterior[0].brightness = 255;
    led_modes_exterior[0].enabled = true;
    
    // Set default LED state
    van_state.leds.roof.enabled = false;
    van_state.leds.roof.current_mode = 0;
    van_state.leds.roof.brightness = 255;
    van_state.leds.exterior.power_enabled = false;
    van_state.leds.exterior.current_mode = 0;
    van_state.leds.exterior.brightness = 255;
    
    ESP_LOGI(TAG, "Protocol initialized");
}

void protocol_update_state(van_state_t *state) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&van_state, state, sizeof(van_state_t));
        xSemaphoreGive(state_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to acquire mutex for state update");
    }
}

van_state_t* protocol_get_state(void) {
    static van_state_t state_copy;
    
#if ENABLE_SIMULATION
    // Update simulated data before returning state
    protocol_simulate_sensor_data();
#endif
    
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Update uptime in main state before copying
        van_state.system.uptime = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        memcpy(&state_copy, &van_state, sizeof(van_state_t));
        xSemaphoreGive(state_mutex);
        return &state_copy;
    }
    ESP_LOGW(TAG, "Failed to acquire mutex for state read");
    return NULL;
}

void protocol_process_command(van_command_t *cmd) {
    if (cmd == NULL) return;
    
    ESP_LOGI(TAG, "Processing command type: %d", cmd->type);
    
    switch (cmd->type) {
        case CMD_SET_HEATER_TARGET:
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (cmd->target == 0) {
                    van_state.heater.target_water_temp = (float)cmd->value / 10.0f;
                    ESP_LOGI(TAG, "Heater water target set to %.1f°C", van_state.heater.target_water_temp);
                } else {
                    van_state.heater.target_cabin_temp = (float)cmd->value / 10.0f;
                    ESP_LOGI(TAG, "Heater cabin target set to %.1f°C", van_state.heater.target_cabin_temp);
                }
                xSemaphoreGive(state_mutex);
            }
            break;
            
        case CMD_SET_HEATER_STATE:
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                van_state.heater.heater_on = (bool)cmd->value;
                ESP_LOGI(TAG, "Heater turned %s", cmd->value ? "ON" : "OFF");
                xSemaphoreGive(state_mutex);
            }
            break;
            
        case CMD_SET_LED_STATE:
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (cmd->target == 0) { // Roof LEDs
                    van_state.leds.roof.enabled = (bool)cmd->value;
                    // If enabling and no mode is set, use default mode 0
                    if (van_state.leds.roof.enabled && van_state.leds.roof.current_mode == 0) {
                        van_state.leds.roof.current_mode = 0; // Default mode
                        if (van_state.leds.roof.brightness == 0) {
                            van_state.leds.roof.brightness = 255; // Default brightness
                        }
                    }
                } else { // Exterior LEDs
                    van_state.leds.exterior.power_enabled = (bool)cmd->value;
                    // If enabling and no mode is set, use default mode 0
                    if (van_state.leds.exterior.power_enabled && van_state.leds.exterior.current_mode == 0) {
                        van_state.leds.exterior.current_mode = 0; // Default mode
                        if (van_state.leds.exterior.brightness == 0) {
                            van_state.leds.exterior.brightness = 255; // Default brightness
                        }
                    }
                }
                ESP_LOGI(TAG, "LED %s state set to %s", 
                         cmd->target == 0 ? "roof" : "exterior", 
                         cmd->value ? "ON" : "OFF");
                xSemaphoreGive(state_mutex);
            }
            break;
            
        case CMD_SET_LED_MODE:
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (cmd->target == 0) { // Roof LEDs
                    van_state.leds.roof.current_mode = cmd->value;
                    ESP_LOGI(TAG, "Roof LED mode set to %d", cmd->value);
                } else { // Exterior LEDs
                    van_state.leds.exterior.current_mode = cmd->value;
                    ESP_LOGI(TAG, "Exterior LED mode set to %d", cmd->value);
                }
                xSemaphoreGive(state_mutex);
            }
            break;
            
        case CMD_SET_LED_BRIGHTNESS:
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (cmd->target == 0) { // Roof LEDs
                    van_state.leds.roof.brightness = cmd->value;
                    ESP_LOGI(TAG, "Roof LED brightness set to %d", cmd->value);
                } else { // Exterior LEDs
                    van_state.leds.exterior.brightness = cmd->value;
                    ESP_LOGI(TAG, "Exterior LED brightness set to %d", cmd->value);
                }
                xSemaphoreGive(state_mutex);
            }
            break;
            
        case CMD_SAVE_LED_MODE:
            if (cmd->target < MAX_LED_MODES) {
                led_mode_t *mode_data = (led_mode_t*)cmd->data;
                if (cmd->value == 0) { // Roof LEDs
                    memcpy(&led_modes_roof[cmd->target], mode_data, sizeof(led_mode_t));
                } else { // Exterior LEDs
                    memcpy(&led_modes_exterior[cmd->target], mode_data, sizeof(led_mode_t));
                }
                ESP_LOGI(TAG, "Saved LED mode %d", cmd->target);
            }
            break;
            
        case CMD_SET_HOOD_STATE:
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                van_state.fans.hood_fan_active = (bool)cmd->value;
                ESP_LOGI(TAG, "Hood fan state set to %s", cmd->value ? "ON" : "OFF");
                xSemaphoreGive(state_mutex);
                
                // Notify fan manager about the change
                fan_manager_set_hood_state((bool)cmd->value);
            }
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown command type: %d", cmd->type);
            break;
    }
}

void protocol_set_error(uint32_t error_code) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Only set the error if it's not already set (avoid spam)
        if (!(van_state.system.error_code & error_code)) {
            van_state.system.system_error = true;
            van_state.system.error_code |= error_code;
            van_state.leds.error_mode_active = true;
            
            // Log which specific error was set
            const char* error_name = "UNKNOWN";
            switch (error_code) {
                case ERROR_HEATER_NO_FUEL: error_name = "HEATER_NO_FUEL"; break;
                case ERROR_MPPT_COMM: error_name = "MPPT_COMM"; break;
                case ERROR_SENSOR_COMM: error_name = "SENSOR_COMM"; break;
                case ERROR_SLAVE_COMM: error_name = "SLAVE_COMM"; break;
                case ERROR_LED_STRIP: error_name = "LED_STRIP"; break;
                case ERROR_FAN_CONTROL: error_name = "FAN_CONTROL"; break;
            }
            
            ESP_LOGE(TAG, "System error set: %s (0x%04X)", 
                    error_name, error_code);
        }
        xSemaphoreGive(state_mutex);
    }
}

void protocol_clear_error_flag(uint32_t error_code) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        van_state.system.error_code &= ~error_code;
        if (van_state.system.error_code == ERROR_NONE) {
            van_state.system.system_error = false;
            van_state.leds.error_mode_active = false;
            ESP_LOGI(TAG, "All system errors cleared");
        } else {
            ESP_LOGI(TAG, "Error flag 0x%04X cleared (remaining: 0x%04X)", error_code, van_state.system.error_code);
        }
        xSemaphoreGive(state_mutex);
    }
}

void protocol_clear_error(void) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        van_state.system.system_error = false;
        van_state.system.error_code = ERROR_NONE;
        van_state.leds.error_mode_active = false;
        xSemaphoreGive(state_mutex);
        ESP_LOGI(TAG, "System errors cleared");
    }
}

// SIMULATION FUNCTIONS - Remove when real hardware is connected
#if ENABLE_SIMULATION
#include <math.h>

void protocol_simulate_sensor_data(void) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint32_t time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t time_s = time_ms / 1000;
        
        // Simulate slow changing values (sin wave over 60 seconds)
        float slow_cycle = (float)(time_s % 60) / 60.0f * 2.0f * M_PI;
        
        // Simulate fast changing values (sin wave over 10 seconds)
        float fast_cycle = (float)(time_s % 10) / 10.0f * 2.0f * M_PI;
        
        // --- SENSOR DATA SIMULATION ---
        // Fuel level: slowly decreasing from 80% to 20%
        van_state.sensors.fuel_level = 50.0f + 30.0f * sinf(slow_cycle);
        
        // Temperatures: realistic van temperatures
        van_state.sensors.onboard_temperature = 25.0f + 10.0f * sinf(slow_cycle);
        van_state.sensors.cabin_temperature = 22.0f + 8.0f * sinf(slow_cycle);
        
        // Humidity: 40-80%
        van_state.sensors.humidity = 60.0f + 20.0f * sinf(slow_cycle);
        
        // CO2: 400-1200 ppm
        van_state.sensors.co2_level = (uint16_t)(800 + 400 * sinf(fast_cycle));
        
        // Light level: 0-1023 (ADC range)
        van_state.sensors.light_level = (uint16_t)(512 + 511 * sinf(fast_cycle));
        
        // Van light: follows light level
        van_state.sensors.van_light_active = van_state.sensors.light_level < 300;
        
        // Door: occasionally open
        van_state.sensors.door_open = (time_s % 30) < 3; // Open 3s every 30s
        
        // --- MPPT DATA SIMULATION ---
        // Solar power varies with "sunlight"
        float solar_factor = fmaxf(0.0f, sinf(slow_cycle));
        
        // MPPT 100|50
        van_state.mppt.solar_power_100_50 = 150.0f * solar_factor;
        van_state.mppt.battery_voltage_100_50 = 12.8f + 0.4f * sinf(slow_cycle);
        van_state.mppt.battery_current_100_50 = van_state.mppt.solar_power_100_50 / van_state.mppt.battery_voltage_100_50;
        van_state.mppt.temperature_100_50 = (int8_t)(30 + 15 * solar_factor);
        van_state.mppt.state_100_50 = solar_factor > 0.1f ? 3 : 1; // 3=Bulk, 1=Off
        
        // MPPT 70|15
        van_state.mppt.solar_power_70_15 = 80.0f * solar_factor;
        van_state.mppt.battery_voltage_70_15 = 12.8f + 0.4f * sinf(slow_cycle);
        van_state.mppt.battery_current_70_15 = van_state.mppt.solar_power_70_15 / van_state.mppt.battery_voltage_70_15;
        van_state.mppt.temperature_70_15 = (int8_t)(28 + 12 * solar_factor);
        van_state.mppt.state_70_15 = solar_factor > 0.1f ? 3 : 1;
        
        // --- HEATER DATA SIMULATION ---
        // IMPORTANT: Only simulate sensor readings, not user-controlled values
        // Target temperatures are set by commands, don't override them!
        
        // Water temperature follows target with some delay (realistic physics simulation)
        static float simulated_water_temp = 20.0f;
        
        // Only simulate heating/cooling when heater is ON (controlled by user commands)
        if (van_state.heater.heater_on) {
            float target = van_state.heater.target_water_temp > 0 ? van_state.heater.target_water_temp : 60.0f;
            float temp_diff = target - simulated_water_temp;
            if (fabs(temp_diff) > 0.5f) {
                simulated_water_temp += temp_diff * 0.02f; // Slow heating
            }
        } else {
            // Cool down towards ambient when heater is off
            float ambient_temp = van_state.sensors.cabin_temperature;
            if (simulated_water_temp > ambient_temp + 2.0f) {
                simulated_water_temp -= 0.1f; // Slow cooling
            }
        }
        van_state.heater.water_temperature = simulated_water_temp;
        
        // Pump and fan states based on heater state (realistic system behavior)
        van_state.heater.pump_active = van_state.heater.heater_on && 
                                      (van_state.heater.target_water_temp > simulated_water_temp + 2.0f);
        
        // Fan speed based on heater activity
        if (van_state.heater.heater_on) {
            van_state.heater.radiator_fan_speed = (uint8_t)(50 + 50 * (simulated_water_temp / 80.0f));
        } else {
            van_state.heater.radiator_fan_speed = 0;
        }
        
        // --- FAN DATA SIMULATION ---
        // Electrical box fan: temperature dependent
        van_state.fans.elec_box_speed = (uint8_t)(30 + 20 * (van_state.sensors.onboard_temperature - 20.0f) / 10.0f);
        if (van_state.fans.elec_box_speed > 100) van_state.fans.elec_box_speed = 100;
        
        // Heater fan: follows heater state
        van_state.fans.heater_fan_speed = van_state.heater.heater_on ? 
                                         (uint8_t)(70 + 30 * sinf(fast_cycle)) : 0;
        
        // Hood fan: occasionally active
        van_state.fans.hood_fan_active = (time_s % 40) < 5; // Active 5s every 40s
        
        // --- LED DATA SIMULATION ---
        // IMPORTANT: Only simulate physical events, not user-controlled states
        
        // Physical switch press simulation (this is a real sensor)
        van_state.leds.roof.switch_pressed = (time_s % 15) == 0; // Pressed every 15s
        if (van_state.leds.roof.switch_pressed) {
            van_state.leds.roof.last_switch_time = time_ms;
        }
        
        // DON'T override user-controlled LED states (enabled, brightness, mode)
        // These are set by commands and should persist until user changes them
        
        // Exterior LEDs can be automatically controlled by van light sensor
        // But only if not manually overridden
        // For now, let's allow automatic control based on light sensor
        van_state.leds.exterior.power_enabled = van_state.sensors.van_light_active;
        
        // --- SYSTEM STATUS SIMULATION ---
        van_state.system.slave_pcb_connected = (time_s % 60) > 5; // Disconnected 5s every 60s
        
        // Simulate occasional errors
        if (van_state.sensors.fuel_level < 10.0f) {
            van_state.system.system_error = true;
            van_state.system.error_code |= ERROR_HEATER_NO_FUEL;
            van_state.leds.error_mode_active = true;
        }
        
        // Clear error when fuel is sufficient
        if (van_state.sensors.fuel_level > 15.0f) {
            van_state.system.error_code &= ~ERROR_HEATER_NO_FUEL;
            if (van_state.system.error_code == ERROR_NONE) {
                van_state.system.system_error = false;
                van_state.leds.error_mode_active = false;
            }
        }
        
        ESP_LOGD(TAG, "Simulated data updated - Fuel: %.1f%%, Solar: %.1fW+%.1fW, Temp: %.1f°C", 
                van_state.sensors.fuel_level, 
                van_state.mppt.solar_power_100_50, 
                van_state.mppt.solar_power_70_15,
                van_state.sensors.cabin_temperature);
        
        xSemaphoreGive(state_mutex);
    }
}
#endif // ENABLE_SIMULATION
