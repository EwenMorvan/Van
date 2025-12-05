/**
 * @file videoprojecteur_manager.c
 * @brief Manager for motorized video projector controlled via BLE
 */

#include "videoprojecteur_manager.h"
#include "../communications/ble/ble_manager_nimble.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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
    float position_percent;    // Last known position (0.0 - 100.0)
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
    // Display raw json data for debugging
    ESP_LOGI(TAG, "📥 Projector JSON data received (%d bytes): %.*s", (int)len, (int)len, (const char*)data);
    
    uint8_t status = data[0];
    
    // Parse status byte according to device protocol
    // If the device sends JSON like {"state":"...","position":xx.xx}, parse it
    // JSON starts with '{' (ASCII 123). Detect that and parse strings instead of reading raw byte.
    if (data[0] == '{') {
        // Copy to a temporary buffer and NUL-terminate
        size_t copy_len = len;
        if (copy_len > 255) copy_len = 255;
        char buf[256];
        memcpy(buf, data, copy_len);
        buf[copy_len] = '\0';

        // Find "state" field
        const char *p_state = strstr(buf, "\"state\"");
        if (p_state) {
            const char *p_col = strchr(p_state, ':');
            if (p_col) {
                // Skip ':' and whitespace
                const char *p = p_col + 1;
                while (*p && isspace((unsigned char)*p)) p++;
                // Expecting a quoted string
                if (*p == '"') {
                    p++;
                    const char *q = strchr(p, '"');
                    if (q) {
                        size_t state_len = q - p;
                        char state_str[64];
                        if (state_len >= sizeof(state_str)) state_len = sizeof(state_str)-1;
                        memcpy(state_str, p, state_len);
                        state_str[state_len] = '\0';

                        // Normalize to lowercase for comparison
                        for (size_t i = 0; i < state_len; i++) state_str[i] = (char)tolower((unsigned char)state_str[i]);

                        if (strstr(state_str, "retracted") != NULL) {
                            g_projector.current_state = PROJECTOR_STATE_RETRACTED;
                            ESP_LOGI(TAG, "📽️  Projector status (json): RETRACTED");
                        } else if (strstr(state_str, "retracting") != NULL) {
                            g_projector.current_state = PROJECTOR_STATE_RETRACTING;
                            ESP_LOGI(TAG, "📽️  Projector status (json): RETRACTING");
                        } else if (strstr(state_str, "deployed") != NULL) {
                            g_projector.current_state = PROJECTOR_STATE_DEPLOYED;
                            ESP_LOGI(TAG, "📽️  Projector status (json): DEPLOYED");
                        } else if (strstr(state_str, "deploying") != NULL) {
                            g_projector.current_state = PROJECTOR_STATE_DEPLOYING;
                            ESP_LOGI(TAG, "📽️  Projector status (json): DEPLOYING");
                        } else if (strstr(state_str, "stopped") != NULL) {
                            g_projector.current_state = PROJECTOR_STATE_STOPPED;
                            ESP_LOGI(TAG, "📽️  Projector status (json): STOPPED");
                        } else {
                            ESP_LOGW(TAG, "Unknown projector state string: %s", state_str);
                            g_projector.current_state = PROJECTOR_STATE_STOPPED;
                        }
                    }
                }
            }
        }

        // Find "position" field (float)
        const char *p_pos = strstr(buf, "\"position\"");
        if (p_pos) {
            const char *p_col = strchr(p_pos, ':');
            if (p_col) {
                const char *p = p_col + 1;
                while (*p && isspace((unsigned char)*p)) p++;
                char *endptr = NULL;
                float pos = strtof(p, &endptr);
                if (endptr && endptr != p) {
                    ESP_LOGI(TAG, "📏 Projector position: %.2f%%", pos);
                    // Store parsed position in local state (clamp 0..100)
                    if (pos < 0.0f) pos = 0.0f;
                    if (pos > 100.0f) pos = 100.0f;
                    g_projector.position_percent = pos;
                }
            }
        }

        return;
    }

}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

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
    g_projector.current_state = PROJECTOR_STATE_STOPPED;
    g_projector.last_status_request_time = 0;
    g_projector.connection_attempts = 0;
    g_projector.position_percent = 0.0f;
    
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
    // Optionally, request initial status after initialization
    ble_request_projector_status(g_projector.mac_address);
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
    
    // Use BLE manager API to send projector command (writes to 0x2A58)
    uint8_t command_byte = (uint8_t)cmd;
    esp_err_t ret = ble_send_projector_command(g_projector.mac_address, command_byte);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "📤 Sent projector command: 0x%02X", command_byte);
    } else {
        ESP_LOGE(TAG, "Failed to send command via BLE manager: %s", esp_err_to_name(ret));
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
    // Store last known position into global van state
    van_state->videoprojecteur.position_percent = g_projector.position_percent;

    // Periodically request status from projector when connected
    if (van_state->videoprojecteur.connected) {
        uint32_t now_ms = esp_timer_get_time() / 1000;
        // Request status every 0.5 seconds
        if (g_projector.last_status_request_time == 0 || (now_ms - g_projector.last_status_request_time) > 500) {
            esp_err_t rc = ble_request_projector_status(g_projector.mac_address);
            if (rc == ESP_OK) {
                g_projector.last_status_request_time = now_ms;
                ESP_LOGD(TAG, "Requested projector status (poll)");
            } else {
                ESP_LOGW(TAG, "Projector status request failed or not supported: %s", esp_err_to_name(rc));
            }
        }

        // Try to read any pending data from BLE manager and parse it
        uint8_t buf[128];
        size_t len = 0;
        if (ble_get_device_data(g_projector.mac_address, buf, sizeof(buf), &len) == ESP_OK && len > 0) {
            // Pass data to parser
            videoprojecteur_on_data_received(g_projector.conn_handle, buf, len);
            // Clear device buffer is handled by BLE manager semantics (next read will return fresh data)
        }
    }
    
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
        case PROJECTOR_CMD_JOG_UP_1_FORCED:
            ESP_LOGI(TAG, "🔼 Jogging up 1.0 turn (forced)");
            break;
        case PROJECTOR_CMD_JOG_DOWN_1_FORCED:
            ESP_LOGI(TAG, "🔽 Jogging down 1.0 turn (forced)");
            break;
        case PROJECTOR_CMD_CALIBRATE_UP:
            ESP_LOGI(TAG, "⚙️  Calibrating projector up");
            break;
        case PROJECTOR_CMD_CALIBRATE_DOWN:
            ESP_LOGI(TAG, "⚙️  Calibrating projector down");
            break;
        default:
            ESP_LOGW(TAG, "Unknown command: %d", cmd->cmd);
            return ESP_ERR_INVALID_ARG;
    }
    
    // Send the command via BLE
    return videoprojecteur_send_command(cmd->cmd);
}
