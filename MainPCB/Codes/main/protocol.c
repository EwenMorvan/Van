#include "protocol.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

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
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
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
                } else {
                    van_state.heater.target_cabin_temp = (float)cmd->value / 10.0f;
                }
                xSemaphoreGive(state_mutex);
            }
            break;
            
        case CMD_SET_HEATER_STATE:
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                van_state.heater.heater_on = (bool)cmd->value;
                xSemaphoreGive(state_mutex);
            }
            break;
            
        case CMD_SET_LED_MODE:
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (cmd->target == 0) { // Roof LEDs
                    van_state.leds.roof.current_mode = cmd->value;
                } else { // Exterior LEDs
                    van_state.leds.exterior.current_mode = cmd->value;
                }
                xSemaphoreGive(state_mutex);
            }
            break;
            
        case CMD_SET_LED_BRIGHTNESS:
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (cmd->target == 0) { // Roof LEDs
                    van_state.leds.roof.brightness = cmd->value;
                } else { // Exterior LEDs
                    van_state.leds.exterior.brightness = cmd->value;
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
            
        default:
            ESP_LOGW(TAG, "Unknown command type: %d", cmd->type);
            break;
    }
}

void protocol_set_error(uint32_t error_code) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        van_state.system.system_error = true;
        van_state.system.error_code |= error_code;
        van_state.leds.error_mode_active = true;
        xSemaphoreGive(state_mutex);
        ESP_LOGE(TAG, "System error set: 0x%04X", error_code);
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
