#include "pump_manager.h"

static const char *TAG = "PUMP_MGR";

static bool pump_current_state = false;

esp_err_t pump_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing pump manager...");

    // Configuration du GPIO pour contrôler la pompe
    gpio_config_t pump_cfg = {
        .pin_bit_mask = (1ULL << PH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t ret = gpio_config(&pump_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Pump GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialiser la pompe à l'état désactivé
    gpio_set_level(PH, 0);

    return ESP_OK;
}

esp_err_t pump_manager_set_state(bool state)
{
    ESP_LOGI(TAG, "Setting pump state to %s", state ? "ENABLED" : "DISABLED");
    pump_current_state = state;
    gpio_set_level(PH, state ? 1 : 0);
    return ESP_OK;
}

bool pump_manager_get_state(void)
{
    int level = gpio_get_level(PH);
    ESP_LOGD(TAG, "Pump GPIO level: %d, reported state: %s", level, pump_current_state ? "ENABLED" : "DISABLED");
    return pump_current_state;
}