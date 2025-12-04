/**
 * @file protocol.c
 * @brief Centralized state management for Van Management System
 * 
 * This module provides a single source of truth for the entire system state.
 * All peripheral managers can read and update their respective sections via pointer access.
 * 
 * Architecture:
 * - Single static instance of van_state_t
 * - Thread-safe access via pointer (managers own their sections)
 */

#include "protocol.h"

static const char *TAG = "PROTOCOL";

// ============================================================================
// STATE VARIABLES - SINGLE INSTANCE
// ============================================================================

/**
 * @brief Global van state - Single source of truth
 * 
 * This structure contains the complete state of all van systems.
 * Each peripheral manager is responsible for updating its own section.
 */
static van_state_t van_state = {0};

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize the protocol module
 * 
 * Initializes the van_state structure with default values.
 * Should be called once during system startup.
 * 
 * @return ESP_OK on success
 */
esp_err_t protocol_init(void) {
    ESP_LOGI(TAG, "Initializing protocol module...");
    
    // Initialize state structure with zeros
    memset(&van_state, 0, sizeof(van_state_t));
    
    
    ESP_LOGI(TAG, "van_state_t size: %d bytes", sizeof(van_state_t));
    ESP_LOGI(TAG, "Protocol module initialized successfully");
    
    return ESP_OK;
}

// ============================================================================
// STATE ACCESS
// ============================================================================

/**
 * @brief Get pointer to the global van state
 * 
 * Returns a direct pointer to the van_state structure.
 * Peripheral managers can use this pointer to read and update their sections.
 * 
 * Example usage in a manager:
 * @code
 * van_state_t* state = protocol_get_van_state();
 * state->heater.fuel_level_percent = 75;
 * state->heater.heater_on = true;
 * @endcode
 * 
 * @return Pointer to van_state_t structure (never NULL)
 */
van_state_t* protocol_get_van_state(void) {
    return &van_state;
}

// ============================================================================
// UTILITY FUNCTIONS (Optional helpers)
// ============================================================================

/**
 * @brief Update system uptime
 * 
 * Helper function to update the system uptime in seconds.
 * Can be called periodically by a maintenance task.
 */
void protocol_update_uptime(void) {
    van_state.system.uptime = (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

/**
 * @brief Get system uptime in seconds
 * 
 * @return Current system uptime in seconds
 */
uint32_t protocol_get_uptime(void) {
    return van_state.system.uptime;
}

/**
 * @brief Set system error state
 * 
 * @param error True if system has error, false otherwise
 * @param error_code Error code (0 if no error)
 */
void protocol_set_system_error(bool error, uint32_t error_code) {
    van_state.system.system_error = error;
    van_state.system.error_code = error_code;
    
    if (error) {
        ESP_LOGW(TAG, "System error set: code 0x%lX", error_code);
    } else {
        ESP_LOGI(TAG, "System error cleared");
    }
}

/**
 * @brief Get current system error state
 * 
 * @param[out] error_code Pointer to store error code (can be NULL)
 * @return True if system has error, false otherwise
 */
bool protocol_has_system_error(uint32_t* error_code) {
    if (error_code != NULL) {
        *error_code = van_state.system.error_code;
    }
    return van_state.system.system_error;
}

// ============================================================================
// DEBUG FUNCTIONS
// ============================================================================

/**
 * @brief Print current van state summary (for debugging)
 * 
 * Prints a human-readable summary of the current system state.
 * Useful for debugging and monitoring.
 */
void protocol_print_state_summary(void) {
    ESP_LOGI(TAG, "=== VAN STATE SUMMARY ===");
    
    // System
    ESP_LOGI(TAG, "System:");
    ESP_LOGI(TAG, "  Uptime: %lu seconds", van_state.system.uptime);
    ESP_LOGI(TAG, "  Error: %s (code: 0x%lX)", 
             van_state.system.system_error ? "YES" : "NO",
             van_state.system.error_code);
    
    // Battery
    ESP_LOGI(TAG, "Battery:");
    ESP_LOGI(TAG, "  Voltage: %.2fV", van_state.battery.voltage_mv / 1000.0f);
    ESP_LOGI(TAG, "  Current: %.2fA", van_state.battery.current_ma / 1000.0f);
    ESP_LOGI(TAG, "  Capacity: %lu mAh", van_state.battery.capacity_mah);
    ESP_LOGI(TAG, "  SOC: %d%%", van_state.battery.soc_percent);
    ESP_LOGI(TAG, "  Cell Count: %d", van_state.battery.cell_count);
    ESP_LOGI(TAG, "  Cycle Count: %d", van_state.battery.cycle_count);
    ESP_LOGI(TAG, "  Health: %d%%", van_state.battery.health_percent);
    
    // MPPT
    ESP_LOGI(TAG, "MPPT:");
    ESP_LOGI(TAG, "  100|50 Power: %.1fW @ %.2fV", 
             van_state.mppt.solar_power_100_50,
             van_state.mppt.battery_voltage_100_50);
    ESP_LOGI(TAG, "  70|15 Power: %.1fW @ %.2fV", 
             van_state.mppt.solar_power_70_15,
             van_state.mppt.battery_voltage_70_15);
    ESP_LOGI(TAG, "  Total Solar: %.1fW", 
             van_state.mppt.solar_power_100_50 + van_state.mppt.solar_power_70_15);
    
    // Sensors
    ESP_LOGI(TAG, "Sensors:");
    ESP_LOGI(TAG, "  Cabin Temp: %.1f°C", van_state.sensors.cabin_temperature);
    ESP_LOGI(TAG, "  Exterior Temp: %.1f°C", van_state.sensors.exterior_temperature);
    ESP_LOGI(TAG, "  Humidity: %.1f%%", van_state.sensors.humidity);
    ESP_LOGI(TAG, "  CO2: %d ppm", van_state.sensors.co2_level);
    ESP_LOGI(TAG, "  Door: %s", van_state.sensors.door_open ? "OPEN" : "CLOSED");
    
    // Heater
    ESP_LOGI(TAG, "Heater:");
    ESP_LOGI(TAG, "  Status: %s", van_state.heater.heater_on ? "ON" : "OFF");
    ESP_LOGI(TAG, "  Target Temp: %.1f°C", van_state.heater.target_air_temperature);
    ESP_LOGI(TAG, "  Actual Temp: %.1f°C", van_state.heater.actual_air_temperature);
    ESP_LOGI(TAG, "  Fuel Level: %d%%", van_state.heater.fuel_level_percent);
    ESP_LOGI(TAG, "  Pump: %s, Fan: %d%%", 
             van_state.heater.pump_active ? "ON" : "OFF",
             van_state.heater.radiator_fan_speed);
    
    // LEDs
    ESP_LOGI(TAG, "LEDs:");
    ESP_LOGI(TAG, "  Roof1: %s, Mode: %d, Brightness: %d", 
             van_state.leds.leds_roof1.enabled ? "ON" : "OFF",
             van_state.leds.leds_roof1.current_mode,
             van_state.leds.leds_roof1.brightness);
    ESP_LOGI(TAG, "  Roof2: %s, Mode: %d, Brightness: %d", 
             van_state.leds.leds_roof2.enabled ? "ON" : "OFF",
             van_state.leds.leds_roof2.current_mode,
             van_state.leds.leds_roof2.brightness);
    ESP_LOGI(TAG, "  Front: %s, Mode: %d, Brightness: %d", 
             van_state.leds.leds_av.enabled ? "ON" : "OFF",
             van_state.leds.leds_av.current_mode,
             van_state.leds.leds_av.brightness);
    ESP_LOGI(TAG, "  Rear: %s, Mode: %d, Brightness: %d", 
             van_state.leds.leds_ar.enabled ? "ON" : "OFF",
             van_state.leds.leds_ar.current_mode,
             van_state.leds.leds_ar.brightness);
    
    ESP_LOGI(TAG, "========================");
}