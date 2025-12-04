#include "app_main_communication_manager.h"

static const char *TAG = "APP_MAIN_COMM_MANAGER";


esp_err_t app_main_communication_manager_init(void) {
    ESP_LOGI(TAG, "Initializing App-Main Communication Manager...");
    
    
    
    ESP_LOGI(TAG, "App-Main Communication Manager initialized successfully");
    return ESP_OK;
}



esp_err_t app_main_send_van_state_to_app(void) {
    if (!ble_is_connected()) {
        ESP_LOGW(TAG, "Cannot send van state: No phone connected");
        return ESP_ERR_INVALID_STATE;
    }

    van_state_t* van_state = protocol_get_van_state();
    if (!van_state) {
        ESP_LOGE(TAG, "Failed to get van state for sending");
        return ESP_ERR_INVALID_STATE;
    }

    // Allouer buffer JSON sur le heap
    char* json_buffer = (char*)malloc(4096);
    if (!json_buffer) {
        ESP_LOGE(TAG, "Failed to allocate JSON buffer!");
        return ESP_ERR_NO_MEM;
    }
    int len = json_build_van_state(van_state, json_buffer, 4096);
    if (len <= 0) {
        ESP_LOGE(TAG, "Failed to build JSON for van state (error: %d)", len);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Sending van state (%d bytes) to %d connected app(s)", len, ble_get_connection_count());
    ble_send_json(json_buffer);

    free(json_buffer);

    return ESP_OK;
}