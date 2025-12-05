/**
 * @file json_builder.c
 * @brief Implementation of JSON builder for van state using cJSON
 */

#include "json_builder.h"

static const char *TAG = "JSON_BUILDER";

int json_build_van_state(const van_state_t* state, char* buffer, size_t buffer_size) {
    if (!state) {
        ESP_LOGE(TAG, "state is NULL");
        return -1;
    }
    if (!buffer) {
        ESP_LOGE(TAG, "buffer is NULL");
        return -1;
    }
    if (buffer_size == 0) {
        ESP_LOGE(TAG, "buffer_size is 0");
        return -1;
    }
    
    // Créer l'objet JSON racine
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create root JSON object");
        return -1;
    }
    // First add a itentidifier to mark the start of the JSON
    // Since it will be fragmentel to pass the BLE MTU limit it is necessary for the app to reconstruct the whole json
    // So begin with "\start_van_state" and end with "\end_van_state"
    cJSON_AddStringToObject(root, "start_van_state", "");
    // ═══════════════════════════════════════════════════════════
    // MPPT - Solar Charge Controllers
    // ═══════════════════════════════════════════════════════════
    cJSON *mppt = cJSON_CreateObject();
    cJSON_AddNumberToObject(mppt, "solar_power_100_50", state->mppt.solar_power_100_50);
    cJSON_AddNumberToObject(mppt, "panel_voltage_100_50", state->mppt.panel_voltage_100_50);
    cJSON_AddNumberToObject(mppt, "panel_current_100_50", state->mppt.panel_current_100_50);
    cJSON_AddNumberToObject(mppt, "battery_voltage_100_50", state->mppt.battery_voltage_100_50);
    cJSON_AddNumberToObject(mppt, "battery_current_100_50", state->mppt.battery_current_100_50);
    cJSON_AddNumberToObject(mppt, "temperature_100_50", state->mppt.temperature_100_50);
    cJSON_AddNumberToObject(mppt, "state_100_50", state->mppt.state_100_50);
    cJSON_AddNumberToObject(mppt, "error_flags_100_50", state->mppt.error_flags_100_50);
    
    cJSON_AddNumberToObject(mppt, "solar_power_70_15", state->mppt.solar_power_70_15);
    cJSON_AddNumberToObject(mppt, "panel_voltage_70_15", state->mppt.panel_voltage_70_15);
    cJSON_AddNumberToObject(mppt, "panel_current_70_15", state->mppt.panel_current_70_15);
    cJSON_AddNumberToObject(mppt, "battery_voltage_70_15", state->mppt.battery_voltage_70_15);
    cJSON_AddNumberToObject(mppt, "battery_current_70_15", state->mppt.battery_current_70_15);
    cJSON_AddNumberToObject(mppt, "temperature_70_15", state->mppt.temperature_70_15);
    cJSON_AddNumberToObject(mppt, "state_70_15", state->mppt.state_70_15);
    cJSON_AddNumberToObject(mppt, "error_flags_70_15", state->mppt.error_flags_70_15);
    cJSON_AddItemToObject(root, "mppt", mppt);
    
    // ═══════════════════════════════════════════════════════════
    // ALTERNATOR/CHARGER
    // ═══════════════════════════════════════════════════════════
    cJSON *alternator_charger = cJSON_CreateObject();
    cJSON_AddNumberToObject(alternator_charger, "state", state->alternator_charger.state);
    cJSON_AddNumberToObject(alternator_charger, "input_voltage", state->alternator_charger.input_voltage);
    cJSON_AddNumberToObject(alternator_charger, "output_voltage", state->alternator_charger.output_voltage);
    cJSON_AddNumberToObject(alternator_charger, "output_current", state->alternator_charger.output_current);
    cJSON_AddItemToObject(root, "alternator_charger", alternator_charger);
    
    // ═══════════════════════════════════════════════════════════
    // INVERTER/CHARGER
    // ═══════════════════════════════════════════════════════════
    cJSON *inverter_charger = cJSON_CreateObject();
    cJSON_AddBoolToObject(inverter_charger, "enabled", state->inverter_charger.enabled);
    cJSON_AddNumberToObject(inverter_charger, "ac_input_voltage", state->inverter_charger.ac_input_voltage);
    cJSON_AddNumberToObject(inverter_charger, "ac_input_frequency", state->inverter_charger.ac_input_frequency);
    cJSON_AddNumberToObject(inverter_charger, "ac_input_current", state->inverter_charger.ac_input_current);
    cJSON_AddNumberToObject(inverter_charger, "ac_input_power", state->inverter_charger.ac_input_power);
    cJSON_AddNumberToObject(inverter_charger, "ac_output_voltage", state->inverter_charger.ac_output_voltage);
    cJSON_AddNumberToObject(inverter_charger, "ac_output_frequency", state->inverter_charger.ac_output_frequency);
    cJSON_AddNumberToObject(inverter_charger, "ac_output_current", state->inverter_charger.ac_output_current);
    cJSON_AddNumberToObject(inverter_charger, "ac_output_power", state->inverter_charger.ac_output_power);
    cJSON_AddNumberToObject(inverter_charger, "battery_voltage", state->inverter_charger.battery_voltage);
    cJSON_AddNumberToObject(inverter_charger, "battery_current", state->inverter_charger.battery_current);
    cJSON_AddNumberToObject(inverter_charger, "inverter_temperature", state->inverter_charger.inverter_temperature);
    cJSON_AddNumberToObject(inverter_charger, "charger_state", state->inverter_charger.charger_state);
    cJSON_AddNumberToObject(inverter_charger, "error_flags", state->inverter_charger.error_flags);
    cJSON_AddItemToObject(root, "inverter_charger", inverter_charger);
    
    // ═══════════════════════════════════════════════════════════
    // BATTERY
    // ═══════════════════════════════════════════════════════════
    cJSON *battery = cJSON_CreateObject();
    cJSON_AddNumberToObject(battery, "voltage_mv", state->battery.voltage_mv);
    cJSON_AddNumberToObject(battery, "current_ma", state->battery.current_ma);
    cJSON_AddNumberToObject(battery, "capacity_mah", state->battery.capacity_mah);
    cJSON_AddNumberToObject(battery, "soc_percent", state->battery.soc_percent);
    cJSON_AddNumberToObject(battery, "cell_count", state->battery.cell_count);
    
    // Cell voltages array
    cJSON *cell_voltages = cJSON_CreateArray();
    for (int i = 0; i < state->battery.cell_count && i < 16; i++) {
        cJSON_AddItemToArray(cell_voltages, cJSON_CreateNumber(state->battery.cell_voltage_mv[i]));
    }
    cJSON_AddItemToObject(battery, "cell_voltage_mv", cell_voltages);
    
    cJSON_AddNumberToObject(battery, "temp_sensor_count", state->battery.temp_sensor_count);
    
    // Temperatures array
    cJSON *temperatures = cJSON_CreateArray();
    for (int i = 0; i < state->battery.temp_sensor_count && i < 8; i++) {
        cJSON_AddItemToArray(temperatures, cJSON_CreateNumber(state->battery.temperatures_c[i]));
    }
    cJSON_AddItemToObject(battery, "temperatures_c", temperatures);
    
    cJSON_AddNumberToObject(battery, "cycle_count", state->battery.cycle_count);
    cJSON_AddNumberToObject(battery, "nominal_capacity_mah", state->battery.nominal_capacity_mah);
    cJSON_AddNumberToObject(battery, "design_capacity_mah", state->battery.design_capacity_mah);
    cJSON_AddNumberToObject(battery, "health_percent", state->battery.health_percent);
    cJSON_AddNumberToObject(battery, "mosfet_status", state->battery.mosfet_status);
    cJSON_AddNumberToObject(battery, "protection_status", state->battery.protection_status);
    cJSON_AddNumberToObject(battery, "balance_status", state->battery.balance_status);
    cJSON_AddItemToObject(root, "battery", battery);
    
    // ═══════════════════════════════════════════════════════════
    // SENSORS
    // ═══════════════════════════════════════════════════════════
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddNumberToObject(sensors, "cabin_temperature", state->sensors.cabin_temperature);
    cJSON_AddNumberToObject(sensors, "exterior_temperature", state->sensors.exterior_temperature);
    cJSON_AddNumberToObject(sensors, "humidity", state->sensors.humidity);
    cJSON_AddNumberToObject(sensors, "co2_level", state->sensors.co2_level);
    cJSON_AddNumberToObject(sensors, "light", state->sensors.light);
    cJSON_AddBoolToObject(sensors, "door_open", state->sensors.door_open);
    cJSON_AddItemToObject(root, "sensors", sensors);
    
    // ═══════════════════════════════════════════════════════════
    // HEATER
    // ═══════════════════════════════════════════════════════════
    cJSON *heater = cJSON_CreateObject();
    cJSON_AddBoolToObject(heater, "heater_on", state->heater.heater_on);
    cJSON_AddNumberToObject(heater, "target_air_temperature", state->heater.target_air_temperature);
    cJSON_AddNumberToObject(heater, "actual_air_temperature", state->heater.actual_air_temperature);
    cJSON_AddNumberToObject(heater, "antifreeze_temperature", state->heater.antifreeze_temperature);
    cJSON_AddNumberToObject(heater, "fuel_level_percent", state->heater.fuel_level_percent);
    cJSON_AddNumberToObject(heater, "error_code", state->heater.error_code);
    cJSON_AddBoolToObject(heater, "pump_active", state->heater.pump_active);
    cJSON_AddNumberToObject(heater, "radiator_fan_speed", state->heater.radiator_fan_speed);
    cJSON_AddItemToObject(root, "heater", heater);
    
    // ═══════════════════════════════════════════════════════════
    // LEDS
    // ═══════════════════════════════════════════════════════════
    cJSON *leds = cJSON_CreateObject();
    
    // LEDs Roof1
    cJSON *leds_roof1 = cJSON_CreateObject();
    cJSON_AddBoolToObject(leds_roof1, "enabled", state->leds.leds_roof1.enabled);
    cJSON_AddNumberToObject(leds_roof1, "current_mode", state->leds.leds_roof1.current_mode);
    cJSON_AddNumberToObject(leds_roof1, "brightness", state->leds.leds_roof1.brightness);
    cJSON_AddItemToObject(leds, "roof1", leds_roof1);

    // LEDs Roof2
    cJSON *leds_roof2 = cJSON_CreateObject();
    cJSON_AddBoolToObject(leds_roof2, "enabled", state->leds.leds_roof2.enabled);
    cJSON_AddNumberToObject(leds_roof2, "current_mode", state->leds.leds_roof2.current_mode);
    cJSON_AddNumberToObject(leds_roof2, "brightness", state->leds.leds_roof2.brightness);
    cJSON_AddItemToObject(leds, "roof2", leds_roof2);
    
    // LEDs Avant
    cJSON *leds_av = cJSON_CreateObject();
    cJSON_AddBoolToObject(leds_av, "enabled", state->leds.leds_av.enabled);
    cJSON_AddNumberToObject(leds_av, "current_mode", state->leds.leds_av.current_mode);
    cJSON_AddNumberToObject(leds_av, "brightness", state->leds.leds_av.brightness);
    cJSON_AddItemToObject(leds, "av", leds_av);
    
    // LEDs Arrière
    cJSON *leds_ar = cJSON_CreateObject();
    cJSON_AddBoolToObject(leds_ar, "enabled", state->leds.leds_ar.enabled);
    cJSON_AddNumberToObject(leds_ar, "current_mode", state->leds.leds_ar.current_mode);
    cJSON_AddNumberToObject(leds_ar, "brightness", state->leds.leds_ar.brightness);
    cJSON_AddItemToObject(leds, "ar", leds_ar);
    
    cJSON_AddItemToObject(root, "leds", leds);
    
    // ═══════════════════════════════════════════════════════════
    // SYSTEM
    // ═══════════════════════════════════════════════════════════
    cJSON *system = cJSON_CreateObject();
    cJSON_AddNumberToObject(system, "uptime", state->system.uptime);
    cJSON_AddBoolToObject(system, "system_error", state->system.system_error);
    cJSON_AddNumberToObject(system, "error_code", state->system.error_code);
    cJSON_AddItemToObject(root, "system", system);
    
    // ═══════════════════════════════════════════════════════════
    // SLAVE PCB
    // ═══════════════════════════════════════════════════════════
    cJSON *slave = cJSON_CreateObject();
    
    // Timestamp & current case
    cJSON_AddNumberToObject(slave, "timestamp", state->slave_pcb.timestamp);
    cJSON_AddNumberToObject(slave, "current_case", state->slave_pcb.current_case);
    cJSON_AddNumberToObject(slave, "hood_state", state->slave_pcb.hood_state);
    
    // Water tanks levels
    cJSON *tanks = cJSON_CreateObject();
    
    cJSON *tank_a = cJSON_CreateObject();
    cJSON_AddNumberToObject(tank_a, "level_percentage", state->slave_pcb.tanks_levels.tank_a.level_percentage);
    cJSON_AddNumberToObject(tank_a, "weight_kg", state->slave_pcb.tanks_levels.tank_a.weight_kg);
    cJSON_AddNumberToObject(tank_a, "volume_liters", state->slave_pcb.tanks_levels.tank_a.volume_liters);
    cJSON_AddItemToObject(tanks, "tank_a", tank_a);
    
    cJSON *tank_b = cJSON_CreateObject();
    cJSON_AddNumberToObject(tank_b, "level_percentage", state->slave_pcb.tanks_levels.tank_b.level_percentage);
    cJSON_AddNumberToObject(tank_b, "weight_kg", state->slave_pcb.tanks_levels.tank_b.weight_kg);
    cJSON_AddNumberToObject(tank_b, "volume_liters", state->slave_pcb.tanks_levels.tank_b.volume_liters);
    cJSON_AddItemToObject(tanks, "tank_b", tank_b);
    
    cJSON *tank_c = cJSON_CreateObject();
    cJSON_AddNumberToObject(tank_c, "level_percentage", state->slave_pcb.tanks_levels.tank_c.level_percentage);
    cJSON_AddNumberToObject(tank_c, "weight_kg", state->slave_pcb.tanks_levels.tank_c.weight_kg);
    cJSON_AddNumberToObject(tank_c, "volume_liters", state->slave_pcb.tanks_levels.tank_c.volume_liters);
    cJSON_AddItemToObject(tanks, "tank_c", tank_c);
    
    cJSON *tank_d = cJSON_CreateObject();
    cJSON_AddNumberToObject(tank_d, "level_percentage", state->slave_pcb.tanks_levels.tank_d.level_percentage);
    cJSON_AddNumberToObject(tank_d, "weight_kg", state->slave_pcb.tanks_levels.tank_d.weight_kg);
    cJSON_AddNumberToObject(tank_d, "volume_liters", state->slave_pcb.tanks_levels.tank_d.volume_liters);
    cJSON_AddItemToObject(tanks, "tank_d", tank_d);
    
    cJSON *tank_e = cJSON_CreateObject();
    cJSON_AddNumberToObject(tank_e, "level_percentage", state->slave_pcb.tanks_levels.tank_e.level_percentage);
    cJSON_AddNumberToObject(tank_e, "weight_kg", state->slave_pcb.tanks_levels.tank_e.weight_kg);
    cJSON_AddNumberToObject(tank_e, "volume_liters", state->slave_pcb.tanks_levels.tank_e.volume_liters);
    cJSON_AddItemToObject(tanks, "tank_e", tank_e);
    
    cJSON_AddItemToObject(slave, "water_tanks", tanks);
    
    // Error state - Statistics
    cJSON *error_state = cJSON_CreateObject();
    
    // Error stats
    cJSON *error_stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(error_stats, "total_errors", state->slave_pcb.error_state.error_stats.total_errors);
    cJSON_AddNumberToObject(error_stats, "last_error_timestamp", state->slave_pcb.error_state.error_stats.last_error_timestamp);
    cJSON_AddNumberToObject(error_stats, "last_error_code", state->slave_pcb.error_state.error_stats.last_error_code);
    
    // Errors by severity array
    cJSON *errors_by_severity = cJSON_CreateArray();
    for (int i = 0; i < 4; i++) {
        cJSON_AddItemToArray(errors_by_severity, cJSON_CreateNumber(state->slave_pcb.error_state.error_stats.errors_by_severity[i]));
    }
    cJSON_AddItemToObject(error_stats, "errors_by_severity", errors_by_severity);
    
    // Errors by category array
    cJSON *errors_by_category = cJSON_CreateArray();
    for (int i = 0; i < 8; i++) {
        cJSON_AddItemToArray(errors_by_category, cJSON_CreateNumber(state->slave_pcb.error_state.error_stats.errors_by_category[i]));
    }
    cJSON_AddItemToObject(error_stats, "errors_by_category", errors_by_category);
    
    cJSON_AddItemToObject(error_state, "stats", error_stats);
    
    // Last errors history (MAX_ERROR_HISTORY = 5)
    cJSON *last_errors = cJSON_CreateArray();
    for (int i = 0; i < 5; i++) {
        cJSON *error_event = cJSON_CreateObject();
        cJSON_AddNumberToObject(error_event, "error_code", state->slave_pcb.error_state.last_errors[i].error_code);
        cJSON_AddNumberToObject(error_event, "severity", state->slave_pcb.error_state.last_errors[i].severity);
        cJSON_AddNumberToObject(error_event, "category", state->slave_pcb.error_state.last_errors[i].category);
        cJSON_AddNumberToObject(error_event, "timestamp", state->slave_pcb.error_state.last_errors[i].timestamp);
        cJSON_AddStringToObject(error_event, "module", state->slave_pcb.error_state.last_errors[i].module);
        cJSON_AddStringToObject(error_event, "description", state->slave_pcb.error_state.last_errors[i].description);
        cJSON_AddNumberToObject(error_event, "data", state->slave_pcb.error_state.last_errors[i].data);
        cJSON_AddItemToArray(last_errors, error_event);
    }
    cJSON_AddItemToObject(error_state, "last_errors", last_errors);
    
    cJSON_AddItemToObject(slave, "error_state", error_state);
    
    // System health
    cJSON *health = cJSON_CreateObject();
    cJSON_AddBoolToObject(health, "system_healthy", state->slave_pcb.system_health.system_healthy);
    cJSON_AddNumberToObject(health, "last_health_check", state->slave_pcb.system_health.last_health_check);
    cJSON_AddNumberToObject(health, "uptime_seconds", state->slave_pcb.system_health.uptime_seconds);
    cJSON_AddNumberToObject(health, "free_heap_size", state->slave_pcb.system_health.free_heap_size);
    cJSON_AddNumberToObject(health, "min_free_heap_size", state->slave_pcb.system_health.min_free_heap_size);
    cJSON_AddItemToObject(slave, "system_health", health);
    
    cJSON_AddItemToObject(root, "slave_pcb", slave);

    // ═══════════════════════════════════════════════════════════
    // VIDEOPROJECTEUR - Motorized video projector
    // ═══════════════════════════════════════════════════════════
    cJSON *videoprojecteur = cJSON_CreateObject();
    cJSON_AddNumberToObject(videoprojecteur, "state", state->videoprojecteur.state);
    cJSON_AddBoolToObject(videoprojecteur, "connected", state->videoprojecteur.connected);
    cJSON_AddNumberToObject(videoprojecteur, "last_update_time", state->videoprojecteur.last_update_time);
    cJSON_AddNumberToObject(videoprojecteur, "position_percent", state->videoprojecteur.position_percent);
    cJSON_AddItemToObject(root, "videoprojecteur", videoprojecteur);

    // Add the end identifier
    cJSON_AddStringToObject(root, "end_van_state", "");
    
    // Convertir en string
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json_str) {
        ESP_LOGE(TAG, "cJSON_PrintUnformatted failed");
        return -1;
    }
    
    // Copier dans le buffer (add trailing newline)
    int written = snprintf(buffer, buffer_size, "%s\n", json_str);
    cJSON_free(json_str);
    
    if (written < 0) {
        ESP_LOGE(TAG, "snprintf failed (returned %d)", written);
        return -1;
    }
    if ((size_t)written >= buffer_size) {
        ESP_LOGE(TAG, "Buffer too small: need %d bytes, have %zu", written + 1, buffer_size);
        return -1;
    }
    
    // ESP_LOGI(TAG, "JSON generated successfully: %d bytes", written);
    return written;
}
