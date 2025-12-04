/**
 * @file json_builder.h
 * @brief Build JSON strings from van_state_t for BLE transmission
 */

#ifndef JSON_BUILDER_H
#define JSON_BUILDER_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"

#include "../communications/protocol.h"
#include "../common_includes/error_manager.h"
#include "../common_includes/slave_pcb_res/slave_pcb_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert van_state_t to compact JSON string
 * 
 * Builds a complete JSON representation of the van state including:
 * - MPPT (2 solar charge controllers: 100|50 and 70|15)
 * - Battery (voltage, current, SOC, temperature, charging state)
 * - Sensors (cabin/exterior temp, humidity, CO2, door)
 * - Heater (state, temps, fuel level, pump, fan)
 * - LEDs (roof, avant, arrière - each with enabled/mode/brightness)
 * - System info (uptime, errors)
 * - Slave PCB state:
 *   - timestamp, current_case, hood_state
 *   - water_tanks (5 tanks: A-E with level/weight/volume)
 *   - error_state:
 *     - stats (total, last error code/timestamp, arrays by severity/category)
 *     - last_errors[] (history of last 5 errors with full details)
 *   - system_health (healthy, uptime, heap stats)
 * 
 * @param state Pointer to van_state_t structure
 * @param buffer Output buffer for JSON string
 * @param buffer_size Size of output buffer (recommended: 4096+ bytes for full state)
 * @return Number of bytes written (excluding null terminator), or -1 on error
 */
int json_build_van_state(const van_state_t* state, char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // JSON_BUILDER_H
