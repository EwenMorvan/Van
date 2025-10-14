#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "log_level.h"
#include "protocol.h"
#include "communication_manager.h"
#include "communication_protocol.h"
#include "uart_multiplexer.h"
#include "sensor_manager.h"
#include "led_manager.h"
#include "heater_manager.h"
#include "mppt_manager.h"
#include "fan_manager.h"
#include "usb_manager.h"
#include "ble_manager_nimble.h"


static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "MainPCB Van Controller starting...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize protocol system
    protocol_init();
    
    // Initialize all managers in order
    ESP_LOGI(TAG, "Initializing UART multiplexer...");
    ESP_ERROR_CHECK(uart_multiplexer_init());
    
    ESP_LOGI(TAG, "Initializing protocol...");
    protocol_init();
    
    ESP_LOGI(TAG, "Initializing communication manager...");
    ESP_ERROR_CHECK(communication_manager_init());
    
    // ESP_LOGI(TAG, "Initializing sensor manager...");
    // ESP_ERROR_CHECK(sensor_manager_init());
    
    // ESP_LOGI(TAG, "Initializing MPPT manager...");
    // ESP_ERROR_CHECK(mppt_manager_init());
    
    ESP_LOGI(TAG, "Initializing fan manager...");
    ESP_ERROR_CHECK(fan_manager_init());
    
    ESP_LOGI(TAG, "Initializing LED manager...");
    ESP_ERROR_CHECK(led_manager_init());
    

    
    // ESP_LOGI(TAG, "Initializing heater manager...");
    // ESP_ERROR_CHECK(heater_manager_init());
    
    ESP_LOGI(TAG, "Initializing communication protocol...");
    ESP_ERROR_CHECK(comm_protocol_init());
    
    // ESP_LOGI(TAG, "Initializing USB manager...");
    // ESP_ERROR_CHECK(usb_manager_init());
    
    // ESP_LOGI(TAG, "Initializing BLE manager...");
    // ESP_ERROR_CHECK(ble_manager_init());
    
    // ESP_LOGI(TAG, "Starting communication protocol task...");
    // ESP_ERROR_CHECK(comm_protocol_start());
    
    ESP_LOGI(TAG, "All managers initialized successfully!");
    ESP_LOGI(TAG, "MainPCB Van Controller is running...");
    
    // Main loop
    while (1) {
        // Monitor system health
        van_state_t *state = protocol_get_state();
        if (state) {
            
            ESP_LOGD(TAG, "System uptime: %lu seconds", state->system.uptime);
            ESP_LOGD(TAG, "Fuel level: %.1f%%", state->sensors.fuel_level);
            ESP_LOGD(TAG, "Cabin temperature: %.1f°C", state->sensors.cabin_temperature);
            ESP_LOGD(TAG, "Heater water temp: %.1f°C", state->heater.water_temperature);
            ESP_LOGD(TAG, "Solar power: %.1fW + %.1fW", 
                    state->mppt.solar_power_100_50, state->mppt.solar_power_70_15);
            
            
            // Check for critical errors
            if (state->system.system_error) {
                ESP_LOGW(TAG, "System error detected:");
                
                // Decode individual errors
                if (state->system.error_code & ERROR_HEATER_NO_FUEL) {
                    ESP_LOGW(TAG, "  - No fuel for heater");
                }
                if (state->system.error_code & ERROR_MPPT_COMM) {
                    ESP_LOGW(TAG, "  - MPPT communication error");
                }
                if (state->system.error_code & ERROR_SENSOR_COMM) {
                    ESP_LOGW(TAG, "  - Sensor communication error");
                }
                if (state->system.error_code & ERROR_SLAVE_COMM) {
                    ESP_LOGW(TAG, "  - Slave PCB communication error");
                }
                if (state->system.error_code & ERROR_LED_STRIP) {
                    ESP_LOGW(TAG, "  - LED strip error");
                }
                if (state->system.error_code & ERROR_FAN_CONTROL) {
                    ESP_LOGW(TAG, "  - Fan control error");
                }
                
                led_trigger_error_mode();
            } else {
                // Periodically check if error conditions are resolved
                static uint32_t last_error_check = 0;
                uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                
                if (current_time - last_error_check > 30000) { // Check every 30 seconds
                    last_error_check = current_time;
                    
                    // Check fuel level for heater error
                    if (state->sensors.fuel_level > 5.0) { // If fuel level is above 5%
                        protocol_clear_error_flag(ERROR_HEATER_NO_FUEL);
                    }
                    
                    ESP_LOGD(TAG, "Periodic error condition check completed");
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Update every 10 seconds
    }
}
