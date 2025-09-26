#ifndef BLE_MANAGER_NIMBLE_H
#define BLE_MANAGER_NIMBLE_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BLE Manager with dual service support
 * 
 * Supports both:
 * - Van Management Service (0xAAA0) for smartphones
 * - ESP32 Communication Service (0x1234) for ESP32 clients
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ble_manager_init(void);

/**
 * @brief Send state data to smartphones (backward compatibility)
 * 
 * @param state_json JSON string containing state information
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ble_manager_send_state(const char* state_json);

/**
 * @brief Send data specifically to ESP32 clients
 * 
 * Uses the ESP32 Communication Service (0x1234) TX characteristic (0x5678)
 * 
 * @param data Data string to send
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ble_manager_send_to_esp32(const char* data);

/**
 * @brief Send data specifically to smartphones
 * 
 * Uses the Van Management Service (0xAAA0) State characteristic (0xAAA2)
 * 
 * @param data Data string to send
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ble_manager_send_to_smartphones(const char* data);

/**
 * @brief Get the number of connected smartphones
 * 
 * @return Number of smartphone connections
 */
int ble_manager_get_smartphone_count(void);

/**
 * @brief Get the number of connected ESP32 clients
 * 
 * @return Number of ESP32 client connections
 */
int ble_manager_get_esp32_count(void);

/**
 * @brief Check if any BLE device is connected
 * 
 * @return true if at least one device is connected, false otherwise
 */
bool ble_manager_is_connected(void);

/**
 * @brief Deinitialize BLE Manager
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ble_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_MANAGER_NIMBLE_H */
</content>
</invoke>
