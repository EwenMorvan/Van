/**
 * @file ble_manager_nimble.h
 * @brief Ultra-simple BLE Manager API with NimBLE
 * 
 * Features:
 * - Multi-device support (up to 4 simultaneous connections)
 * - Automatic fragmentation for large JSON (no size limit)
 * - External BLE device management by MAC address
 * - Battery services (standard + extended)
 * - Thread-safe operations
 */

#ifndef BLE_MANAGER_NIMBLE_H
#define BLE_MANAGER_NIMBLE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "../protocol.h"


#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CALLBACK TYPES
// ============================================================================

/**
 * @brief Callback function for received data from smartphone app
 * @param conn_handle BLE connection handle (identifies which device sent the data)
 * @param data Raw data received from app
 * @param length Data length in bytes
 */
typedef void (*ble_receive_callback_t)(uint16_t conn_handle, const uint8_t* data, size_t length);

// ============================================================================
// INITIALIZATION & CONNECTION
// ============================================================================

/**
 * @brief Initialize BLE manager with multi-device support
 * 
 * Initializes NimBLE stack and creates GATT services for van management.
 * Supports up to 4 simultaneous connections (smartphones + devices)
 * 
 * @param receive_callback Callback called when data is received from app (can be NULL)
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
esp_err_t ble_init(ble_receive_callback_t receive_callback);

/**
 * @brief Check if at least one BLE client is connected
 * @return true if connected, false otherwise
 */
bool ble_is_connected(void);

/**
 * @brief Get number of active BLE connections
 * @return Number of connected devices (0-4)
 */
uint8_t ble_get_connection_count(void);

// ============================================================================
// DATA TRANSMISSION (Automatic Fragmentation)
// ============================================================================

/**
 * @brief Send raw data to all connected devices
 * 
 * Automatically fragments large messages (>500 bytes) into chunks.
 * Safe for any data size.
 * 
 * @param data Raw data buffer
 * @param length Data length in bytes
 * @return ESP_OK on success
 */
esp_err_t ble_send_raw(const uint8_t* data, size_t length);

/**
 * @brief Send JSON string to all connected devices (NO SIZE LIMIT)
 * 
 * Automatically fragments if JSON is larger than 500 bytes.
 * Perfect for large state objects.
 * 
 * Example:
 * @code
 * ble_send_json("{\"battery\":{\"soc\":85.5,\"voltage\":13.2}}");
 * @endcode
 * 
 * @param json_string JSON string (null-terminated)
 * @return ESP_OK on success
 */
esp_err_t ble_send_json(const char* json_string);

// ============================================================================
// EXTERNAL DEVICE MANAGEMENT
// ============================================================================

/**
 * @brief Add external BLE device by MAC address and auto-connect
 * 
 * Register external BLE devices for connection:
 * - BLE battery monitors
 * - External sensors
 * - Other BLE peripherals
 * 
 * Automatically starts scanning and connecting to the device.
 * Use ble_is_device_connected() to check connection status.
 * Use ble_get_device_data() to read received data.
 * 
 * Example:
 * @code
 * uint8_t battery_mac[] = {0xA4, 0xC1, 0x37, 0x15, 0x13, 0x85};
 * ble_add_device_by_mac(battery_mac, "BatteryMonitor");
 * 
 * // Later, check connection and read data
 * if (ble_is_device_connected(battery_mac)) {
 *     uint8_t buf[256];
 *     size_t len = 0;
 *     if (ble_get_device_data(battery_mac, buf, sizeof(buf), &len) == ESP_OK) {
 *         ESP_LOG_BUFFER_HEX("DATA", buf, len);
 *     }
 * }
 * @endcode
 * 
 * @param mac_address 6-byte MAC address
 * @param device_name Friendly name for logging (can be NULL)
 * @return ESP_OK on success
 */
esp_err_t ble_add_device_by_mac(const uint8_t mac_address[6], const char* device_name);

/**
 * @brief Start BLE scan for external devices manually
 * 
 * Starts scanning for registered external devices. The scan uses optimized
 * parameters to reduce interference with LEDs/RMT.
 * 
 * @return ESP_OK on success
 */
esp_err_t ble_start_external_scan(void);

/**
 * @brief Stop BLE scan for external devices
 * 
 * @return ESP_OK on success
 */
esp_err_t ble_stop_external_scan(void);

/**
 * @brief Remove external device by MAC address
 * @param mac_address 6-byte MAC address
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if not registered
 */
esp_err_t ble_remove_device_by_mac(const uint8_t mac_address[6]);

/**
 * @brief Check if specific external device is connected
 * @param mac_address 6-byte MAC address
 * @return true if connected, false otherwise
 */
bool ble_is_device_connected(const uint8_t mac_address[6]);

/**
 * @brief Print data received from external device(s)
 * 
 * Displays received data from external devices for debugging.
 * 
 * @param mac_address Device MAC address (NULL to print all devices)
 */
void ble_print_device_data(const uint8_t mac_address[6]);

/**
 * @brief Read latest data received from a registered external device
 *
 * Copies up to buf_size bytes into out_buffer and returns actual length in out_len.
 * If the device isn't found, returns ESP_ERR_NOT_FOUND.
 *
 * @param mac_address 6-byte MAC address
 * @param out_buffer Destination buffer for data
 * @param buf_size Size of destination buffer
 * @param out_len Pointer to size_t where the actual data length will be stored
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND, ESP_ERR_INVALID_ARG, or ESP_ERR_NO_MEM
 */
esp_err_t ble_get_device_data(const uint8_t mac_address[6], uint8_t* out_buffer, size_t buf_size, size_t* out_len);

/**
 * @brief Request fresh battery data from external device (XiaoXiang/JBD BMS)
 * 
 * Sends JBD command 0x03 (hardware info) to get voltage, current, capacity, SoC, etc.
 * If JBD characteristics (0xFF01/0xFF02) are detected, uses proper protocol.
 * Otherwise falls back to direct read on handle 16.
 * 
 * After calling this, wait ~200-500ms then use ble_get_device_data() to retrieve the response.
 * 
 * Example:
 * @code
 * uint8_t battery_mac[] = {0x85, 0x13, 0x15, 0x37, 0xC1, 0xA4};
 * ble_request_battery_update(battery_mac);
 * vTaskDelay(pdMS_TO_TICKS(300));  // Wait for response
 * 
 * uint8_t buf[64];
 * size_t len = 0;
 * ble_get_device_data(battery_mac, buf, sizeof(buf), &len);
 * // Parse buf with battery_parse_data()
 * @endcode
 * 
 * @param mac_address 6-byte MAC address of battery device
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if device not found/connected
 */
esp_err_t ble_request_battery_update(const uint8_t mac_address[6]);

/**
 * @brief Request individual cell voltages from XiaoXiang/JBD BMS
 * 
 * Sends JBD command 0x04 (cell info) to get voltage of each cell.
 * Only works if JBD characteristics are detected.
 * 
 * @param mac_address 6-byte MAC address of battery device
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if device not found/connected
 */
esp_err_t ble_request_battery_cells(const uint8_t mac_address[6]);

/**
 * @brief Send raw data to an external device via GATT write
 * 
 * Writes data to a specific attribute handle on an external BLE device.
 * This is a generic write function for custom BLE protocols.
 * 
 * @param mac_address MAC address of the external device
 * @param attr_handle Attribute handle to write to
 * @param data Pointer to data to send
 * @param len Length of data
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if device not connected
 */
esp_err_t ble_write_to_external_device(const uint8_t mac_address[6], 
                                      uint16_t attr_handle,
                                      const uint8_t* data, 
                                      size_t len);

/**
 * @brief Request video projector status (reads characteristic 0x2A19)
 * @param mac_address 6-byte MAC address of the projector
 */
esp_err_t ble_request_projector_status(const uint8_t mac_address[6]);

/**
 * @brief Send a projector command (writes to characteristic 0x2A58)
 * @param mac_address 6-byte MAC address of the projector
 * @param cmd Projector command (see `projector_command_t` in `protocol.h`)
 */
esp_err_t ble_send_projector_command(const uint8_t mac_address[6], uint8_t cmd);

// ============================================================================
// DEBUG & MONITORING
// ============================================================================

/**
 * @brief Print current BLE manager status
 * 
 * Displays:
 * - Connection count
 * - Connected devices with MAC addresses
 * - GATT service handles
 * - Battery state
 */
void ble_print_status(void);

/**
 * @brief Deinitialize BLE manager
 * 
 * Disconnects all devices and stops BLE stack.
 * 
 * @return ESP_OK on success
 */
esp_err_t ble_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // BLE_MANAGER_NIMBLE_H
