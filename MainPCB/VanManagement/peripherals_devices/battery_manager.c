#include "battery_manager.h"
#include "../common_includes/simulation_config.h"
#ifdef ENABLE_ENERGY_SIMULATION
#include "energy_simulation.h"
#endif

const static char* TAG = "BATTERY_MANAGER";

#define DEBUG_BATTERY_DATA 0

#if defined(ENABLE_ENERGY_SIMULATION) && ENABLE_ENERGY_SIMULATION
#define SIMULATE_BATTERY_DATA 1 // Set to 1 to simulate battery data for testing
#else
#define SIMULATE_BATTERY_DATA 0
#endif

#define BATTERY_DEVICE_NOMINAL_CAPACITY_MAH 300000  // 300 Ah factory design capacity
// NOTE: BLE address is little-endian (reversed byte order)
// If app displays A4:C1:37:15:13:85, in code it's 85:13:15:37:C1:A4
static const uint8_t BATTERY_DEVICE_MAC[] = {0x85, 0x13, 0x15, 0x37, 0xC1, 0xA4};

esp_err_t battery_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Battery Manager...");

    // Register external battery for auto-connection
    ESP_LOGD(TAG, "🔋 Registering battery device...");
    esp_err_t ret_bat = ble_add_device_by_mac(BATTERY_DEVICE_MAC, "BatteryMonitor");
    if (ret_bat == ESP_OK) {
        ESP_LOGD(TAG, "✅ Battery device registered, will auto-connect");
        ESP_LOGI(TAG, "Battery Manager initialized successfully");
            
        return ESP_OK;

    } else {
        ESP_LOGE(TAG, "❌ Failed to register battery device");
        return ret_bat;
    }

}

esp_err_t battery_manager_update_van_state(van_state_t* van_state){
    #ifndef SIMULATE_BATTERY_DATA
        battery_data_t battery = battery_manager_read_battery_data(); 
    
    #else
        // Use shared simulation context
        energy_simulation_context_t* sim_ctx = energy_simulation_get_context();
        
        // Battery state evolves based on net current from energy system
        float battery_net_current_a = sim_ctx->battery_net_current_a;
        
        // Integrate current to update SOC (20ms update rate = 0.02s)
        // ΔAh = I * Δt / 3600 (convert seconds to hours)
        // Battery capacity = 300 Ah
        float delta_ah = battery_net_current_a * 0.02f / 3600.0f;
        float delta_soc_percent = (delta_ah / 300.0f) * 100.0f;
        
        sim_ctx->battery_soc_percent += delta_soc_percent;
        sim_ctx->battery_soc_percent = fmaxf(0.0f, fminf(100.0f, sim_ctx->battery_soc_percent));
        
        float current_soc = sim_ctx->battery_soc_percent;
        
        battery_data_t battery = {0};
        battery.valid = 1;
        
        // Voltage depends on SOC (LiFePO4 curve: 3.0V-3.4V per cell, 12V-13.6V for 4S)
        // 0% SOC ≈ 12.0V, 50% SOC ≈ 12.8V, 100% SOC ≈ 13.6V
        float base_voltage = 12.0f + (current_soc / 100.0f) * 1.6f;
        
        // Voltage drop/rise based on current (internal resistance ~5mΩ)
        float voltage_delta = -battery_net_current_a * 0.005f; // -I*R
        
        sim_ctx->battery_voltage_v = base_voltage + voltage_delta;
        sim_ctx->battery_voltage_v = fmaxf(10.0f, fminf(14.5f, sim_ctx->battery_voltage_v));
        
        battery.voltage_mv = (uint16_t)(sim_ctx->battery_voltage_v * 1000.0f);
        
        // Current from energy balance (calculated by inverter_chargers_manager)
        battery.current_ma = (int16_t)(battery_net_current_a * 1000.0f);
        
        // Capacité restante cohérente avec le SOC
        battery.capacity_mah = (current_soc / 100.0f) * 290000;
        battery.soc_percent = (uint8_t)current_soc;
        
        battery.cell_count = 4;
        
        // Cellules avec légères variations individuelles (each ~3.0V-3.4V)
        float avg_cell_voltage = sim_ctx->battery_voltage_v / 4.0f;
        for (int i = 0; i < 4; i++) {
            float cell_variation = sinf(sim_ctx->time_ticks * 0.1f + i * 0.5f) * 0.05f; // ±50mV
            battery.cell_voltage_mv[i] = (uint16_t)((avg_cell_voltage + cell_variation + (i * 0.01f)) * 1000.0f);
            battery.cell_voltage_mv[i] = (uint16_t)fmaxf(2800.0f, fminf(3600.0f, (float)battery.cell_voltage_mv[i]));
        }
        
        battery.temp_sensor_count = 2;
        
        // Températures qui varient avec le courant
        float temp_base = 25.0f + (fabsf(battery_net_current_a) / 30.0f) * 15.0f; // Heat from current
        battery.temperatures_c[0] = temp_base + sinf(sim_ctx->time_ticks * 0.05f) * 2.0f;
        battery.temperatures_c[1] = temp_base + sinf(sim_ctx->time_ticks * 0.05f + 1.0f) * 1.5f;
        
        // Cycle count qui augmente très lentement
        battery.cycle_count = 120 + (sim_ctx->time_ticks / 1000);
        
        // Capacité nominale qui diminue très lentement (vieillissement)
        battery.nominal_capacity_mah = 290000 - (sim_ctx->time_ticks / 100) * 10;
        battery.design_capacity_mah = BATTERY_DEVICE_NOMINAL_CAPACITY_MAH;
        
        // Santé de la batterie qui diminue très lentement
        battery.health_percent = 96 - (sim_ctx->time_ticks / 2000);
        battery.health_percent = fmaxf(70, battery.health_percent);
        
        // Status MOSFETs - changent avec la direction de charge
        battery.mosfet_status = (battery_net_current_a > 0.1f) ? 0b01 : 0b10;
        
        // Protection status - occasionnellement active
        battery.protection_status = (sim_ctx->time_ticks % 500 == 0) ? 0x01 : 0x00;
        
        // Balance status qui change dynamiquement
        battery.balance_status = (1 << (sim_ctx->time_ticks % 4)); // Balance une cellule différente à chaque fois
    
    #endif

    if(!van_state || battery.valid == 0){
        return ESP_ERR_INVALID_ARG;
    }
    // Basic measurements
    van_state->battery.voltage_mv = battery.voltage_mv; // millivolts
    van_state->battery.current_ma = battery.current_ma; // milliamps
    van_state->battery.capacity_mah = battery.capacity_mah; // milliamp-hours
    van_state->battery.soc_percent = battery.soc_percent; // uint8_t percent

    // Cell information
    van_state->battery.cell_count = battery.cell_count; // number of cells
    for(uint8_t i = 0; i < battery.cell_count; i++){
        van_state->battery.cell_voltage_mv[i] = battery.cell_voltage_mv[i]; 
    }

    // Temperature sensors
    van_state->battery.temp_sensor_count = battery.temp_sensor_count; // Number of temperature sensors
    for(uint8_t i = 0; i < battery.temp_sensor_count; i++){
        van_state->battery.temperatures_c[i] = battery.temperatures_c[i]; 
    }

    // Additional info
    van_state->battery.cycle_count = battery.cycle_count;       // Number of full charge/discharge cycles
    van_state->battery.nominal_capacity_mah = battery.nominal_capacity_mah;  // Current full capacity measured by BMS (can degrade over time)
    van_state->battery.design_capacity_mah = battery.design_capacity_mah;   // Factory design capacity (e.g., 300,000 mAh = 300 Ah)
    van_state->battery.health_percent = battery.health_percent;     // Battery health (nominal / design × 100%)
    van_state->battery.mosfet_status = battery.mosfet_status;       // Bit 0=charge MOSFET, Bit 1=discharge MOSFET
    van_state->battery.protection_status = battery.protection_status; // Protection flags (overvoltage, undervoltage, etc.)
    van_state->battery.balance_status = battery.balance_status;     // Cell balance status bitfield

    return ESP_OK;
}

battery_data_t battery_manager_read_battery_data(void) {
    battery_data_t battery = {0};
    battery.design_capacity_mah = BATTERY_DEVICE_NOMINAL_CAPACITY_MAH;  // 300 Ah factory design capacity

    // Request fresh battery data and display it
    if (ble_is_device_connected(BATTERY_DEVICE_MAC)) {
        
        
        // Step 1: Request basic info (command 0x03)
        if (ble_request_battery_update(BATTERY_DEVICE_MAC) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(200));  // Wait for response
            
            uint8_t buf[256];
            size_t len = 0;
            if (ble_get_device_data(BATTERY_DEVICE_MAC, buf, sizeof(buf), &len) == ESP_OK && len > 0) {
                battery_parse_data(buf, len, &battery);
            }
        }
        
        // Step 2: Request cell voltages (command 0x04)
        if (ble_request_battery_cells(BATTERY_DEVICE_MAC) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(200));  // Wait for response
            
            uint8_t buf[256];
            size_t len = 0;
            if (ble_get_device_data(BATTERY_DEVICE_MAC, buf, sizeof(buf), &len) == ESP_OK && len > 0) {
                battery_parse_cell_voltages(buf, len, &battery);
            }
        }
        
        // Display complete battery status if debug enabled
        #if DEBUG_BATTERY_DATA
            if (battery.valid) {
                battery_print_data(&battery);
            }
        #endif

    } else {
        
       ESP_LOGW(TAG, "Battery device is not connected...");
    }
    return battery;
}