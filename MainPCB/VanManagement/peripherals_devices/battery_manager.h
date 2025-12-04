#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "esp_err.h"
#include "esp_log.h"

#include "../communications/ble/ble_manager_nimble.h"
#include "../utils/battery_parser.h"
#include "../communications/protocol.h"


esp_err_t battery_manager_init(void);

/**
 * @brief Update van_state_t structure with parsed battery data
 * @param van_state Pointer to van_state_t structure to update
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t battery_manager_update_van_state(van_state_t* van_state);


battery_data_t battery_manager_read_battery_data(void);
#endif // BATTERY_MANAGER_H