#include "hood_manager.h"

static const char *TAG = "HOOD_MGR";

// Initialize hood control
esp_err_t hood_init(void) {
    // Configure hood fan control pin (digital on/off)
    gpio_config_t hood_fan_config = {
        .pin_bit_mask = (1ULL << HOOD_FAN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&hood_fan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure hood fan pin");
        return ret;
    }
    // Ensure hood fan is off initially
    gpio_set_level(HOOD_FAN, 0);

    // Initialize GPIO for hood control if needed
    ESP_LOGI(TAG, "Hood manager initialized");
    return ESP_OK;
}

// Set hood state (on/off)
void hood_set_state(hood_state_t state) {
    ESP_LOGI(TAG, "Setting hood fan state to %s", state == HOOD_ON ? "ON" : "OFF");
    if (state == HOOD_ON) {
        // Turn on hood fan at full speed (100%)
        gpio_set_level(HOOD_FAN, 1);

    } else {
        // Turn off hood fan
        gpio_set_level(HOOD_FAN, 0);
    }
}