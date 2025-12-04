/**
 * @file videoprojecteur_manager.c
 * @brief Manager for motorized video projector controlled via BLE
 */

#include "videoprojecteur_manager.h"
#include "../communications/ble/ble_manager_nimble.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "\033[0;36mPROJ_MGR\033[0m";

// ============================================================================
// DEVICE STATE
// ============================================================================

typedef struct {
    uint8_t mac_address[6];
    bool initialized;
    projector_state_t current_state;
    uint32_t last_status_request_time;
    uint8_t connection_attempts;
    uint16_t conn_handle;      // BLE connection handle when connected
    bool ble_connected;        // Whether BLE is actually connected
    uint16_t ctrl_attr_handle; // Attribute handle for control characteristic (0x2A58)
} videoprojecteur_state_t;

static videoprojecteur_state_t g_projector = {0};

// ============================================================================
// CALLBACK - BLE DATA RECEPTION
// ============================================================================

static void videoprojecteur_on_data_received(uint16_t conn_handle, const uint8_t* data, size_t len) {
    if (len == 0) {
        ESP_LOGD(TAG, "Empty status received");
        return;
    }
    
    uint8_t status = data[0];
    
    // Parse status byte according to device protocol
    switch (status) {
        case 0:
            g_projector.current_state = PROJECTOR_STATE_RETRACTED;
            ESP_LOGI(TAG, "📽️  Projector status: RETRACTED");
            break;
        case 1:
            g_projector.current_state = PROJECTOR_STATE_RETRACTING;
            ESP_LOGI(TAG, "📽️  Projector status: RETRACTING");
            break;
        case 2:
            g_projector.current_state = PROJECTOR_STATE_DEPLOYED;
            ESP_LOGI(TAG, "📽️  Projector status: DEPLOYED");
            break;
        case 3:
            g_projector.current_state = PROJECTOR_STATE_DEPLOYING;
            ESP_LOGI(TAG, "📽️  Projector status: DEPLOYING");
            break;
        default:
            ESP_LOGW(TAG, "Unknown projector status: %d", status);
            g_projector.current_state = PROJECTOR_STATE_UNKNOWN;
            break;
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Convert device name to MAC address
 * Note: For now, we need to hardcode the MAC or get it from config
 * This function can be improved to support device scanning
 */
static void get_videoprojecteur_mac(uint8_t mac[6]) {
    //This MAC is static since the projector has a fixed address
    mac[0] = 0x46;
    mac[1] = 0x9B;
    mac[2] = 0xA7;
    mac[3] = 0x81;
    mac[4] = 0x8C;
    mac[5] = 0x58;
    
    ESP_LOGW(TAG, "⚠️  Using default MAC [%02X:%02X:%02X:%02X:%02X:%02X] - MUST be configured!",
             mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
}

// ============================================================================
// PUBLIC API
// ============================================================================

esp_err_t videoprojecteur_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Video Projector Manager...");
    
    if (g_projector.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    // Get projector MAC address (needs to be configured)
    get_videoprojecteur_mac(g_projector.mac_address);
    
    // Initialize state
    g_projector.current_state = PROJECTOR_STATE_UNKNOWN;
    g_projector.last_status_request_time = 0;
    g_projector.connection_attempts = 0;
    
    // Register device with BLE manager for external device scanning
    // Note: The BLE callback will be handled through ble_get_device_data()
    esp_err_t ret = ble_add_device_by_mac(g_projector.mac_address, VIDEOPROJECTEUR_DEVICE_NAME);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register projector with BLE manager: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Video Projector Manager initialized");
    ESP_LOGI(TAG, "📽️  Device: %s [%02X:%02X:%02X:%02X:%02X:%02X]",
             VIDEOPROJECTEUR_DEVICE_NAME,
             g_projector.mac_address[0], g_projector.mac_address[1],
             g_projector.mac_address[2], g_projector.mac_address[3],
             g_projector.mac_address[4], g_projector.mac_address[5]);
    
    g_projector.initialized = true;
    return ESP_OK;
}

esp_err_t videoprojecteur_send_command(projector_command_t cmd) {
    if (!g_projector.initialized) {
        ESP_LOGE(TAG, "Manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!ble_is_device_connected(g_projector.mac_address)) {
        ESP_LOGE(TAG, "❌ Projector not connected");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Create command byte
    uint8_t command_byte = (uint8_t)cmd;
    
    // Send via BLE write to the projector
    // Note: For proper implementation, we need to:
    // 1. Discover the actual attribute handle for characteristic 0x2A58
    // 2. Or use a default handle based on the BLE device structure
    // For now, we'll use the ble_write_to_external_device function
    // The actual handle might need to be discovered and cached
    
    // TODO: Get the actual attribute handle from GATT discovery
    // For now, use a placeholder or hardcoded handle
    uint16_t ctrl_handle = g_projector.ctrl_attr_handle;
    if (ctrl_handle == 0) {
        ESP_LOGW(TAG, "⚠️  Control handle not discovered yet, will retry after discovery");
        return ESP_FAIL;
    }
    
    esp_err_t ret = ble_write_to_external_device(g_projector.mac_address,
                                                 ctrl_handle,
                                                 &command_byte,
                                                 1);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "📤 Sent projector command: 0x%02X", command_byte);
    } else {
        ESP_LOGE(TAG, "Failed to send command: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t videoprojecteur_manager_update_van_state(van_state_t* van_state) {
    if (!van_state) {
        return ESP_ERR_INVALID_ARG;
    }
    
    van_state->videoprojecteur.state = g_projector.current_state;
    van_state->videoprojecteur.connected = ble_is_device_connected(g_projector.mac_address);
    van_state->videoprojecteur.last_update_time = esp_timer_get_time() / 1000; // Convert to ms
    
    return ESP_OK;
}

bool videoprojecteur_is_connected(void) {
    return ble_is_device_connected(g_projector.mac_address);
}

projector_state_t videoprojecteur_get_state(void) {
    return g_projector.current_state;
}

esp_err_t videoprojecteur_request_status(void) {
    if (!g_projector.initialized) {
        ESP_LOGE(TAG, "Manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Send GET_STATUS command (0x03)
    return videoprojecteur_send_command(PROJECTOR_CMD_GET_STATUS);
}

// ============================================================================
// COMMAND HANDLER
// ============================================================================

/**
 * @brief Handle video projector commands from the app
 * Called by main.c when a COMMAND_TYPE_VIDEOPROJECTEUR is received
 */
esp_err_t videoprojecteur_apply_command(const videoprojecteur_command_t* cmd) {
    if (!cmd) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!videoprojecteur_is_connected()) {
        ESP_LOGE(TAG, "Projector not connected, ignoring command");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Applying projector command: %d", cmd->cmd);
    
    switch (cmd->cmd) {
        case PROJECTOR_CMD_DEPLOY:
            ESP_LOGI(TAG, "🔽 Deploying projector...");
            break;
        case PROJECTOR_CMD_RETRACT:
            ESP_LOGI(TAG, "🔼 Retracting projector...");
            break;
        case PROJECTOR_CMD_STOP:
            ESP_LOGI(TAG, "⏹️  Stopping projector motor");
            break;
        case PROJECTOR_CMD_GET_STATUS:
            ESP_LOGI(TAG, "📊 Requesting projector status");
            break;
        case PROJECTOR_CMD_JOG_UP_1:
            ESP_LOGI(TAG, "🔼 Jogging up 1.0 turn");
            break;
        case PROJECTOR_CMD_JOG_UP_01:
            ESP_LOGI(TAG, "🔼 Jogging up 0.1 turn");
            break;
        case PROJECTOR_CMD_JOG_UP_001:
            ESP_LOGI(TAG, "🔼 Jogging up 0.01 turn");
            break;
        case PROJECTOR_CMD_JOG_DOWN_1:
            ESP_LOGI(TAG, "🔽 Jogging down 1.0 turn");
            break;
        case PROJECTOR_CMD_JOG_DOWN_01:
            ESP_LOGI(TAG, "🔽 Jogging down 0.1 turn");
            break;
        case PROJECTOR_CMD_JOG_DOWN_001:
            ESP_LOGI(TAG, "🔽 Jogging down 0.01 turn");
            break;
        default:
            ESP_LOGW(TAG, "Unknown command: %d", cmd->cmd);
            return ESP_ERR_INVALID_ARG;
    }
    
    // Send the command via BLE
    return videoprojecteur_send_command(cmd->cmd);
}
