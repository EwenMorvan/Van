/**
 * @file battery_parser.c
 * @brief Parser implementation for XiaoXiang/JBD BMS (Overkill Solar)
 * 
 * Protocol: XiaoXiang/JBD BMS over BLE
 * Model: SP04S034L4S200A
 * 
 * Data format for 0x03 command response (basic info):
 * Byte 0-1: Header (0xDD 0x03)
 * Byte 2: Status (0x00 = OK, 0x80 = error)
 * Byte 3: Data length (0x1B = 27 bytes)
 * Byte 4-5: Total voltage (0.01V units, big-endian)
 * Byte 6-7: Current (0.01A units, big-endian, signed)
 * Byte 8-9: Remaining capacity (0.01Ah units, big-endian)
 * Byte 10-11: Nominal capacity (0.01Ah units, big-endian)
 * Byte 12-13: Cycle count (big-endian)
 * Byte 14-15: Production date (packed format)
 * Byte 16-19: Balance status (bitfield)
 * Byte 20-21: Protection status (bitfield)
 * Byte 22: Software version (0.1 units)
 * Byte 23: State of charge (percentage 0-100)
 * Byte 24: MOSFET status (bit 0=charge, bit 1=discharge)
 * Byte 25: Number of cells
 * Byte 26: Number of temp sensors
 * Byte 27+: Temperature data (0.1K units - 2731, big-endian per sensor)
 * Last 3 bytes: Checksum (2 bytes) + end marker (0x77)
 * 
 * Reference: https://github.com/FurTrader/OverkillSolarBMS
 */

#include "battery_parser.h"


static const char* TAG = "BATTERY_PARSER";

// Helper to read 16-bit big-endian value (XiaoXiang uses big-endian!)
static inline uint16_t read_u16_be(const uint8_t* data) {
    return ((uint16_t)data[0] << 8) | (uint16_t)data[1];
}

// Helper to read 16-bit big-endian signed value
static inline int16_t read_i16_be(const uint8_t* data) {
    return (int16_t)read_u16_be(data);
}

bool battery_parse_data(const uint8_t* raw_data, size_t length, battery_data_t* out_battery) {
    if (!raw_data || !out_battery || length < 23) {  // Minimum: header(4) + data(15) + checksum(2) + end(1) + padding = 23
        ESP_LOGE(TAG, "Invalid parameters or data too short (need >= 23 bytes, got %u)", length);
        return false;
    }
    
    // Verify JBD protocol header
    if (raw_data[0] != 0xDD || raw_data[3] < 0x1B) {
        ESP_LOGE(TAG, "Invalid JBD header: first byte=0x%02x (expected 0xDD), length byte=0x%02x", 
                 raw_data[0], raw_data[3]);
        return false;
    }
    
    // Save design_capacity_mah before clearing (set by caller)
    uint32_t saved_design_capacity = out_battery->design_capacity_mah;
    
    memset(out_battery, 0, sizeof(battery_data_t));
    memcpy(out_battery->raw_data, raw_data, (length > 19 ? 19 : length));
    
    // Restore design_capacity_mah
    out_battery->design_capacity_mah = saved_design_capacity;
    
    // XiaoXiang/JBD BMS Protocol structure:
    // [DD][CMD][STATUS][LEN][...DATA...][CHECKSUM_H][CHECKSUM_L][77]
    // Skip 4-byte header: DD 03 00 1D
    const uint8_t* data = &raw_data[4];  // Start of actual data payload
    
    // Bytes 0-1 (offset 4-5 from start): Total voltage in 0.01V units (big-endian)
    // Example: 05 a6 = 1446 decimal = 14.46V
    uint16_t raw_voltage_centiv = read_u16_be(&data[0]);
    out_battery->voltage_mv = raw_voltage_centiv * 10;  // Convert 0.01V to mV
    
    // Bytes 2-3 (offset 6-7): Current in 0.01A units (big-endian, signed)
    // Example: 00 53 = 83 decimal = 0.83A
    int16_t raw_current_centia = read_i16_be(&data[2]);
    out_battery->current_ma = raw_current_centia * 10;  // Convert 0.01A to mA
    
    // Bytes 4-5 (offset 8-9): Remaining capacity in 0.01Ah units (big-endian)
    // Example: 75 30 = 30000 decimal = 300.00Ah
    uint16_t raw_remain_ah = read_u16_be(&data[4]);
    out_battery->capacity_mah = raw_remain_ah * 10;  // Convert 0.01Ah to mAh
    
    // Bytes 6-7 (offset 10-11): Nominal capacity in 0.01Ah units (big-endian)
    // Example: 75 30 = 30000 decimal = 300.00Ah
    uint16_t raw_nominal_ah = read_u16_be(&data[6]);
    out_battery->nominal_capacity_mah = (uint32_t)raw_nominal_ah * 10;
    
    // Bytes 8-9 (offset 12-13): Cycle count (big-endian)
    // Example: 00 03 = 3 cycles
    out_battery->cycle_count = read_u16_be(&data[8]);
    
    // Bytes 10-11 (offset 14-15): Production date (skip for now)
    
    // Bytes 12-15 (offset 16-19): Balance status (4 bytes, little-endian bitfield)
    out_battery->balance_status = ((uint32_t)data[15] << 24) | 
                                   ((uint32_t)data[14] << 16) | 
                                   ((uint32_t)data[13] << 8) | 
                                   data[12];
    
    // Bytes 16-17 (offset 20-21): Protection status (big-endian)
    out_battery->protection_status = read_u16_be(&data[16]);
    
    // Byte 18 (offset 22): Software version (0.1 units)
    out_battery->software_version = data[18];
    
    // Byte 19 (offset 23): State of Charge (percentage 0-100)
    out_battery->soc_percent = data[19];
    if (out_battery->soc_percent > 100) out_battery->soc_percent = 100;
    
    // Byte 20 (offset 24): MOSFET status
    out_battery->mosfet_status = data[20];
    
    // Byte 21 (offset 25): Number of cells
    out_battery->cell_count = data[21];
    if (out_battery->cell_count > 16) out_battery->cell_count = 16;
    
    // Byte 22 (offset 26): Number of temp sensors
    out_battery->temp_sensor_count = data[22];
    if (out_battery->temp_sensor_count > 8) out_battery->temp_sensor_count = 8;
    
    // Bytes 23+ (offset 27+): Temperature data (2 bytes per sensor, big-endian, 0.1K - 2731)
    uint8_t data_len = raw_data[3];  // Length field from header
    uint8_t temp_bytes_available = (data_len > 23) ? (data_len - 23) : 0;
    uint8_t num_temps_readable = temp_bytes_available / 2;
    if (num_temps_readable > out_battery->temp_sensor_count) {
        num_temps_readable = out_battery->temp_sensor_count;
    }
    
    for (uint8_t i = 0; i < num_temps_readable; i++) {
        uint16_t raw_temp = read_u16_be(&data[23 + (i * 2)]);
        // Convert: 0.1K units, offset by 2731 (273.1K = 0°C)
        out_battery->temperatures_c[i] = (int16_t)((raw_temp - 2731) / 10);
    }
    
    // Health calculation: Compare current full capacity (nominal) to factory design capacity
    // Health = (nominal_capacity_mah / design_capacity_mah) × 100%
    // If design_capacity_mah not set (0), assume battery is healthy (100%)
    if (out_battery->design_capacity_mah > 0 && out_battery->nominal_capacity_mah > 0) {
        // Calculate actual health based on degradation
        uint32_t health = (out_battery->nominal_capacity_mah * 100) / out_battery->design_capacity_mah;
        out_battery->health_percent = (health > 100) ? 100 : (uint8_t)health;
    } else {
        // Design capacity not provided or invalid, assume 100% health
        out_battery->health_percent = 100;
    }
    
    // Validation
    bool voltage_valid = (out_battery->voltage_mv > 1000 && out_battery->voltage_mv < 60000);  
    bool soc_valid = (out_battery->soc_percent <= 100);
    
    out_battery->valid = voltage_valid && soc_valid;
    
    return out_battery->valid;
}

bool battery_parse_cell_voltages(const uint8_t* raw_data, size_t length, battery_data_t* out_battery) {
    if (!raw_data || !out_battery || length < 7) {
        ESP_LOGE(TAG, "Invalid parameters for cell voltage parsing");
        return false;
    }
    
    // Verify JBD protocol header for command 0x04
    if (raw_data[0] != 0xDD || raw_data[1] != 0x04) {
        ESP_LOGE(TAG, "Invalid JBD cell voltage response: expected DD 04, got %02X %02X", 
                 raw_data[0], raw_data[1]);
        return false;
    }
    
    uint8_t data_len = raw_data[3];  // Number of data bytes
    if (length < (size_t)(4 + data_len + 3)) {  // Header(4) + data + checksum(2) + end(1)
        ESP_LOGE(TAG, "Cell voltage response too short: got %u, expected %u", 
                 length, 4 + data_len + 3);
        return false;
    }
    
    // Data starts at offset 4
    const uint8_t* data = &raw_data[4];
    
    // Number of cells = data_len / 2 (each cell voltage is 2 bytes)
    out_battery->cell_count = data_len / 2;
    if (out_battery->cell_count > 16) {
        out_battery->cell_count = 16;  // Cap at array size
    }
    
    // Parse cell voltages (big-endian, in mV)
    for (uint8_t i = 0; i < out_battery->cell_count; i++) {
        out_battery->cell_voltage_mv[i] = read_u16_be(&data[i * 2]);
    }
    
    ESP_LOGD(TAG, "✅ Parsed %u cell voltages", out_battery->cell_count);
    return true;
}


void battery_print_data(const battery_data_t* battery) {
    if (!battery) {
        return;
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║          🔋  BMS SP04S034L4S200A                  ║");
    ESP_LOGI(TAG, "╠═══════════════════════════════════════════════════╣");
    
    if (battery->valid) {
        // Basic measurements
        ESP_LOGI(TAG, "║ 📊 MEASUREMENTS                                   ║");
        ESP_LOGI(TAG, "║   Voltage:      %5u mV  (%6.2f V)              ║", 
                 battery->voltage_mv, battery->voltage_mv / 1000.0);
        ESP_LOGI(TAG, "║   Current:      %5d mA  (%6.2f A)              ║", 
                 battery->current_ma, battery->current_ma / 1000.0);
        ESP_LOGI(TAG, "║   State of Charge:          %3u %%                 ║", battery->soc_percent);
        ESP_LOGI(TAG, "║                                                   ║");
        
        // Capacity info
        ESP_LOGI(TAG, "║ 🔋 CAPACITY                                       ║");
        ESP_LOGI(TAG, "║   Remaining:   %6lu mAh (%7.2f Ah)            ║", 
                 (unsigned long)battery->capacity_mah, battery->capacity_mah / 1000.0);
        ESP_LOGI(TAG, "║   Full Charge: %6lu mAh (%7.2f Ah) [BMS]      ║", 
                 (unsigned long)battery->nominal_capacity_mah, battery->nominal_capacity_mah / 1000.0);
        if (battery->design_capacity_mah > 0) {
            ESP_LOGI(TAG, "║   Design:      %6lu mAh (%7.2f Ah) [Usine]    ║", 
                     (unsigned long)battery->design_capacity_mah, battery->design_capacity_mah / 1000.0);
        }
        ESP_LOGI(TAG, "║   Cycle Count: %5u (charge/discharge cycles)    ║", battery->cycle_count);
        if (battery->design_capacity_mah > 0) {
            ESP_LOGI(TAG, "║   Health:      %3u%% (dégradation: %.1f Ah)         ║", 
                     battery->health_percent, 
                     (battery->design_capacity_mah - battery->nominal_capacity_mah) / 1000.0);
        } else {
            ESP_LOGI(TAG, "║   Health:      %3u%% (nominal capacity)            ║", battery->health_percent);
        }
        ESP_LOGI(TAG, "║                                                   ║");
        
        // Status
        ESP_LOGI(TAG, "║ ⚙️  STATUS                                        ║");
        if (battery->current_ma > 100) {
            ESP_LOGI(TAG, "║   Mode: ⚡ CHARGING                               ║");
        } else if (battery->current_ma < -100) {
            ESP_LOGI(TAG, "║   Mode: 🔋 DISCHARGING                           ║");
        } else {
            ESP_LOGI(TAG, "║   Mode: ⏸️  IDLE                                  ║");
        }
        ESP_LOGI(TAG, "║   MOSFET: %s | %s                           ║",
                 (battery->mosfet_status & 0x01) ? "CHG✅" : "CHG❌",
                 (battery->mosfet_status & 0x02) ? "DSG✅" : "DSG❌");
        ESP_LOGI(TAG, "║   Software Version: v%u.%u                          ║",
                 battery->software_version / 10, battery->software_version % 10);
        ESP_LOGI(TAG, "║   Protection: 0x%04X                              ║", battery->protection_status);
        ESP_LOGI(TAG, "║                                                   ║");
        
        // Temperature sensors (if available)
        if (battery->temp_sensor_count > 0) {
            ESP_LOGI(TAG, "║ 🌡️  TEMPERATURES                                ║");
            for (uint8_t i = 0; i < battery->temp_sensor_count; i++) {
                ESP_LOGI(TAG, "║   Sensor %u: %3d °C                                ║", 
                         i + 1, battery->temperatures_c[i]);
            }
            ESP_LOGI(TAG, "║                                                   ║");
        }
        
        // Cell voltages (if available)
        if (battery->cell_count > 0) {
            ESP_LOGI(TAG, "║ 🔋 CELL VOLTAGES (%u cells)                        ║", battery->cell_count);
            
            // Calculate min, max, and delta
            uint16_t min_cell_mv = 65535, max_cell_mv = 0;
            for (uint8_t i = 0; i < battery->cell_count; i++) {
                if (battery->cell_voltage_mv[i] < min_cell_mv) min_cell_mv = battery->cell_voltage_mv[i];
                if (battery->cell_voltage_mv[i] > max_cell_mv) max_cell_mv = battery->cell_voltage_mv[i];
            }
            uint16_t delta_mv = max_cell_mv - min_cell_mv;
            
            for (uint8_t i = 0; i < battery->cell_count; i++) {
                ESP_LOGI(TAG, "║   Cell %2u: %4u mV  (%5.3f V)                     ║", 
                         i + 1, battery->cell_voltage_mv[i], battery->cell_voltage_mv[i] / 1000.0);
            }
            ESP_LOGI(TAG, "║   Delta:   %4u mV  (max - min)                   ║", delta_mv);
            if (delta_mv > 100) {
                ESP_LOGI(TAG, "║   ⚠️  High cell imbalance! Consider balancing.  ║");
            }
        }
        
        // Explanation of metrics
        // ESP_LOGI(TAG, "╠════════════════════════════════════════════════════╣");
        // ESP_LOGI(TAG, "║ ℹ️  INFO                                           ║");
        // ESP_LOGI(TAG, "║   • Cycles: Number of complete charge/discharge    ║");
        // ESP_LOGI(TAG, "║     cycles (battery lifespan indicator)            ║");
        // ESP_LOGI(TAG, "║   • Health: Measured capacity vs factory capacity  ║");
        // ESP_LOGI(TAG, "║     (100%% = no degradation, <80%% = replace)       ║");
        // ESP_LOGI(TAG, "║   • Full Charge (BMS): Max measured capacity       ║");
        // ESP_LOGI(TAG, "║   • Design (Factory): Original nominal capacity    ║");
        // ESP_LOGI(TAG, "║   • MOSFET CHG: Enables charging                   ║");
        // ESP_LOGI(TAG, "║   • MOSFET DSG: Enables discharging                ║");
    } else {
        ESP_LOGI(TAG, "║ ⚠️  INVALID DATA - Check connection              ║");
    }
    
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
}
