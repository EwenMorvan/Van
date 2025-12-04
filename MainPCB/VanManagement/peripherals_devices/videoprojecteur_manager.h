/**
 * @file videoprojecteur_manager.h
 * @brief Manager for motorized video projector controlled via BLE
 * 
 * Features:
 * - Connects to external ESP32 controlling the motorized video projector
 * - Sends deploy/retract/jog commands via BLE
 * - Monitors projector state (Retracted/Deploying/Deployed/Retracting)
 * - Updates global van state with projector status
 */

#ifndef VIDEOPROJECTEUR_MANAGER_H
#define VIDEOPROJECTEUR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "../communications/protocol.h"

// ============================================================================
// DEVICE INFORMATION
// ============================================================================

#define VIDEOPROJECTEUR_DEVICE_NAME "VideoProjector_Van"
#define VIDEOPROJECTEUR_SERVICE_UUID 0x181A
#define VIDEOPROJECTEUR_CTRL_CHAR_UUID 0x2A58
#define VIDEOPROJECTEUR_STATUS_CHAR_UUID 0x2A19

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Initialize the video projector manager
 * Registers the device with BLE manager and sets up callbacks
 * @return ESP_OK on success
 */
esp_err_t videoprojecteur_manager_init(void);

/**
 * @brief Send a command to the video projector
 * @param cmd The command to send (deploy, retract, jog, etc.)
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if device not connected
 */
esp_err_t videoprojecteur_send_command(projector_command_t cmd);

/**
 * @brief Update van state with current projector status
 * Should be called from main update loop
 * @param van_state Pointer to van state structure
 * @return ESP_OK on success
 */
esp_err_t videoprojecteur_manager_update_van_state(van_state_t* van_state);

/**
 * @brief Check if projector device is connected
 * @return True if device is connected via BLE
 */
bool videoprojecteur_is_connected(void);

/**
 * @brief Get current projector state
 * @return Current state (Retracted/Deploying/Deployed/Retracting)
 */
projector_state_t videoprojecteur_get_state(void);

/**
 * @brief Request status update from projector (GET_STATUS command)
 * @return ESP_OK on success
 */
esp_err_t videoprojecteur_request_status(void);

/**
 * @brief Apply a command to the video projector
 * Called from command handler when COMMAND_TYPE_VIDEOPROJECTEUR is received
 * @param cmd Pointer to videoprojecteur command
 * @return ESP_OK on success
 */
esp_err_t videoprojecteur_apply_command(const videoprojecteur_command_t* cmd);

#endif // VIDEOPROJECTEUR_MANAGER_H
