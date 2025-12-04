#ifndef LED_COMMAND_HANDLER_H
#define LED_COMMAND_HANDLER_H

#include "esp_err.h"
#include "../communications/protocol.h"

/**
 * @brief Apply LED command received from BLE app
 * 
 * This function interprets a van_command_t and applies the LED configuration
 * to the appropriate LED strips. It handles both static and dynamic modes.
 * 
 * @param cmd Pointer to the van_command_t structure containing LED command
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_apply_command(const van_command_t* cmd);

#endif // LED_COMMAND_HANDLER_H
