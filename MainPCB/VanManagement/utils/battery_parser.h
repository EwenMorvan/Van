/**
 * @file battery_parser.h
 * @brief Parser for external BLE battery monitor data
 * 
 * Decodes 19-byte battery data from service 0xff00, handle 16
 */

#ifndef BATTERY_PARSER_H
#define BATTERY_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "esp_log.h"
#include "esp_err.h"

#include "../communications/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parsed battery data structure
 */
typedef struct {
    // Status
    bool valid;                 // True if data was successfully parsed
    
    // Basic measurements (JBD/XiaoXiang BMS protocol)
    uint16_t voltage_mv;        // Total battery voltage in millivolts (e.g., 13200 = 13.2V)
    int16_t current_ma;         // Battery current in milliamps (positive = charging, negative = discharging)
    uint8_t soc_percent;        // State of Charge in percent (0-100%)
    uint32_t capacity_mah;      // Remaining capacity in mAh (can be > 65535 for large batteries!)
    
    // Cell information (from command 0x04)
    uint8_t cell_count;         // Number of cells (usually 3, 4, 7, 8, 13, or 16)
    uint16_t cell_voltage_mv[16];  // Individual cell voltages in mV (max 16 cells)
    
    // Temperature sensors (from command 0x03)
    uint8_t temp_sensor_count;  // Number of temperature sensors
    int16_t temperatures_c[8];  // Temperature values in Celsius (max 8 sensors)
    
    // Additional info
    uint16_t cycle_count;       // Number of full charge/discharge cycles
    uint32_t nominal_capacity_mah;  // Current full capacity measured by BMS (can degrade over time)
    uint32_t design_capacity_mah;   // Factory design capacity (e.g., 300,000 mAh = 300 Ah)
    uint8_t health_percent;     // Battery health (nominal / design × 100%)
    
    // Status flags (from command 0x03)
    uint8_t software_version;   // BMS software version (× 0.1)
    uint8_t mosfet_status;      // Bit 0=charge MOSFET, Bit 1=discharge MOSFET
    uint16_t protection_status; // Protection flags (overvoltage, undervoltage, etc.)
    uint32_t balance_status;    // Cell balance status bitfield
    
    // Raw data for debugging
    uint8_t raw_data[19];
    
} battery_data_t;

/**
 * @brief Parse raw battery data from JBD BMS (command 0x03 response)
 * 
 * Decodes the JBD protocol response containing voltage, current, capacity, temperatures, etc.
 * 
 * Example:
 * @code
 * uint8_t raw[36] = {0xDD, 0x03, 0x00, 0x1D, ...};
 * battery_data_t battery;
 * battery.design_capacity_mah = 300000;  // 300 Ah battery
 * if (battery_parse_data(raw, 36, &battery)) {
 *     ESP_LOGI(TAG, "Voltage: %.2f V", battery.voltage_mv / 1000.0);
 *     ESP_LOGI(TAG, "Health: %u%%", battery.health_percent);
 * }
 * @endcode
 * 
 * @param raw_data Raw JBD response data (DD 03 00 LEN [data...] CHKSUM 77)
 * @param length Length of raw data
 * @param out_battery Pointer to battery_data_t structure to fill
 * @return true if parsing succeeded, false otherwise
 * 
 * @note Set out_battery->design_capacity_mah BEFORE calling this function to get accurate health %
 */
bool battery_parse_data(const uint8_t* raw_data, size_t length, battery_data_t* out_battery);

/**
 * @brief Parse cell voltages from JBD command 0x04 response
 * 
 * Updates the cell_count and cell_voltage_mv fields in battery_data_t.
 * 
 * @param raw_data Raw JBD response data (DD 04 00 LEN [cells...] CHKSUM 77)
 * @param length Length of raw data
 * @param out_battery Pointer to battery_data_t structure to update
 * @return true if parsing succeeded, false otherwise
 */
bool battery_parse_cell_voltages(const uint8_t* raw_data, size_t length, battery_data_t* out_battery);


/**
 * @brief Print parsed battery data to console
 * 
 * @param battery Pointer to parsed battery data
 */
void battery_print_data(const battery_data_t* battery);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_PARSER_H
