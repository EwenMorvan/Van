#ifndef INVERTER_CHARGERS_MANAGER_H
#define INVERTER_CHARGERS_MANAGER_H

#include "esp_err.h"
#include "../communications/protocol.h"

/**
 * @brief Initialize inverter/chargers manager
 * @return ESP_OK on success
 */
esp_err_t inverter_chargers_manager_init(void);

/**
 * @brief Update van_state with inverter/chargers data and energy flow simulation
 * 
 * This function simulates a complete energy management system:
 * - Multiplus 12/800 (inverter/charger from AC mains)
 * - Orion-Tr Smart 12/12-30A (DC-DC charger from alternator)
 * - 12V DC loads (max 500W)
 * - 220V AC loads via inverter (max 1kW)
 * - Battery charging/discharging based on energy balance
 * 
 * Energy flows are calculated coherently:
 * - Solar power from MPPTs
 * - AC mains charging when available
 * - Alternator charging when engine running
 * - Direct power routing (bypass battery when possible)
 * - Battery absorbs surplus or provides deficit
 * 
 * @param van_state Pointer to van_state structure to update
 * @return ESP_OK on success
 */
esp_err_t inverter_chargers_manager_update_van_state(van_state_t* van_state);

#endif // INVERTER_CHARGERS_MANAGER_H
