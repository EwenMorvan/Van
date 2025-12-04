/**
 * @file inverter_chargers_manager.c
 * @brief Energy management system - Inverter/Chargers simulation
 * 
 * Simulates complete energy flows in the van:
 * - Multiplus 12/800: Inverter/charger (230V AC ↔ 12V DC)
 * - Orion-Tr Smart 12/12-30A: DC-DC charger (alternator → battery)
 * - Energy balance with battery as buffer
 * - Realistic load simulation (12V + 220V devices)
 * 
 * Only compiled when ENABLE_ENERGY_SIMULATION is defined
 */

#include "inverter_chargers_manager.h"
#include "../common_includes/simulation_config.h"
#ifdef ENABLE_ENERGY_SIMULATION
#include "energy_simulation.h"
#endif
#include "esp_log.h"
#include <math.h>

static const char *TAG = "INVERTER_CHARGERS_MGR";

#if defined(ENABLE_ENERGY_SIMULATION) && ENABLE_ENERGY_SIMULATION
#define SIMULATE_ENERGY_FLOWS 1  // Set to 1 to simulate energy flows
#else
#define SIMULATE_ENERGY_FLOWS 0
#endif

// ============================================================================
// DEVICE SPECIFICATIONS
// ============================================================================

// Multiplus 12/800 specs
#define MULTIPLUS_MAX_CHARGE_CURRENT_A    50.0f    // Max charging current from AC
#define MULTIPLUS_MAX_INVERTER_POWER_W    800.0f   // Max continuous inverter power
#define MULTIPLUS_AC_INPUT_VOLTAGE        230.0f   // Nominal AC input
#define MULTIPLUS_EFFICIENCY_CHARGE       0.92f    // Charging efficiency
#define MULTIPLUS_EFFICIENCY_INVERTER     0.88f    // Inverter efficiency

// Orion-Tr Smart 12/12-30A specs
#define ORION_MAX_OUTPUT_CURRENT_A        30.0f    // Max charging current
#define ORION_INPUT_VOLTAGE_MIN           11.0f    // Min input voltage
#define ORION_INPUT_VOLTAGE_MAX           15.0f    // Max input voltage
#define ORION_EFFICIENCY                  0.95f    // DC-DC efficiency

// Load limits
#define MAX_12V_LOAD_W                    500.0f   // Max 12V devices load
#define MAX_220V_LOAD_W                   1000.0f  // Max 220V devices load

// Battery limits
#define BATTERY_MIN_VOLTAGE_V             11.5f    // Min safe voltage
#define BATTERY_MAX_VOLTAGE_V             14.4f    // Max charge voltage
#define BATTERY_NOMINAL_VOLTAGE_V         12.8f    // Nominal voltage

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Generate random float in range [min, max]
 */
static float random_float(float min, float max) {
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

/**
 * @brief Simulate AC mains availability (random but persistent for periods)
 */
static bool simulate_ac_mains_available(uint32_t time) {
    // AC available in blocks of ~5 minutes (20% probability)
    uint32_t block = time / 300; // 5-minute blocks
    return (block % 5) == 0; // Available 1 out of 5 blocks
}

/**
 * @brief Simulate engine running (alternator available)
 */
static bool simulate_engine_running(uint32_t time) {
    // Engine runs in blocks of ~3 minutes (15% probability)
    uint32_t block = time / 180; // 3-minute blocks
    return (block % 7) == 0; // Running 1 out of 7 blocks
}

/**
 * @brief Calculate battery charge state based on voltage
 */
static charge_state_t calculate_charge_state(float battery_voltage_v) {
    if (battery_voltage_v < 12.0f) {
        return CHARGE_STATE_BULK;
    } else if (battery_voltage_v < 13.8f) {
        return CHARGE_STATE_BULK;
    } else if (battery_voltage_v < 14.2f) {
        return CHARGE_STATE_ABSORPTION;
    } else {
        return CHARGE_STATE_FLOAT;
    }
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

esp_err_t inverter_chargers_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Inverter/Chargers Manager...");
    ESP_LOGI(TAG, "✅ Energy simulation initialized");
    ESP_LOGI(TAG, "  - Multiplus 12/800 (inverter/charger)");
    ESP_LOGI(TAG, "  - Orion-Tr Smart 12/12-30A (alternator charger)");
    ESP_LOGI(TAG, "  - 12V loads: max %.0fW", MAX_12V_LOAD_W);
    ESP_LOGI(TAG, "  - 220V loads: max %.0fW", MAX_220V_LOAD_W);
    return ESP_OK;
}

esp_err_t inverter_chargers_manager_update_van_state(van_state_t* van_state) {
    if (!van_state) {
        return ESP_ERR_INVALID_ARG;
    }
    
#ifdef SIMULATE_ENERGY_FLOWS
    // Use shared simulation context
    energy_simulation_context_t* sim_ctx = energy_simulation_get_context();
    
    // ═══════════════════════════════════════════════════════════
    // STEP 1: Get battery state from shared context
    // ═══════════════════════════════════════════════════════════
    float battery_voltage_v = sim_ctx->battery_voltage_v;
    if (battery_voltage_v < 10.0f || battery_voltage_v > 16.0f) {
        battery_voltage_v = BATTERY_NOMINAL_VOLTAGE_V; // Default if not initialized
    }
    
    float battery_soc = sim_ctx->battery_soc_percent;
    
    // ═══════════════════════════════════════════════════════════
    // STEP 2: Get solar power from shared context (calculated by MPPT manager)
    // ═══════════════════════════════════════════════════════════
    float solar_power_total_w = van_state->mppt.solar_power_100_50 + 
                                 van_state->mppt.solar_power_70_15;
    float solar_current_total_a = sim_ctx->solar_current_a;
    
    // ═══════════════════════════════════════════════════════════
    // STEP 3: Simulate ALTERNATOR CHARGER (Orion-Tr Smart)
    // ═══════════════════════════════════════════════════════════
    bool engine_running = sim_ctx->engine_running;
    float alternator_voltage_v = 0.0f;
    float alternator_output_current_a = 0.0f;
    float alternator_output_voltage_v = 0.0f;
    
    if (engine_running && battery_soc < 95.0f) {
        // Engine running: alternator provides ~14V
        alternator_voltage_v = 14.2f + random_float(-0.2f, 0.2f);
        
        // Charging current depends on battery state
        float charge_current_max = ORION_MAX_OUTPUT_CURRENT_A;
        if (battery_voltage_v > 14.0f) {
            charge_current_max *= 0.5f; // Reduce in float stage
        }
        
        alternator_output_current_a = random_float(charge_current_max * 0.7f, charge_current_max);
        alternator_output_voltage_v = battery_voltage_v + 0.2f; // Slightly higher than battery
    }
    
    float alternator_power_w = alternator_output_voltage_v * alternator_output_current_a;
    
    // Update alternator_charger state
    van_state->alternator_charger.state = engine_running ? 
                                          calculate_charge_state(battery_voltage_v) : 
                                          CHARGE_STATE_OFF;
    van_state->alternator_charger.input_voltage = alternator_voltage_v;
    van_state->alternator_charger.output_voltage = alternator_output_voltage_v;
    van_state->alternator_charger.output_current = alternator_output_current_a;
    
    // ═══════════════════════════════════════════════════════════
    // STEP 4: Simulate LOADS (12V devices + 220V via inverter)
    // ═══════════════════════════════════════════════════════════
    
    // 12V loads: random consumption with daily pattern
    float time_of_day = fmodf(sim_ctx->time_ticks * 0.01f, 24.0f); // 24h cycle
    float load_12v_factor = 0.3f + 0.5f * sinf(time_of_day * 0.26f); // Peak in evening
    float load_12v_w = random_float(50.0f, MAX_12V_LOAD_W * load_12v_factor);
    float load_12v_a = load_12v_w / battery_voltage_v;
    
    // 220V loads: occasional usage (cooking, laptop charging, etc.)
    bool inverter_needed = (sim_ctx->time_ticks % 100) < 30; // 30% of the time
    float load_220v_w = 0.0f;
    float inverter_dc_input_w = 0.0f;
    float inverter_dc_input_a = 0.0f;
    
    if (inverter_needed && battery_voltage_v > BATTERY_MIN_VOLTAGE_V) {
        load_220v_w = random_float(100.0f, MAX_220V_LOAD_W * 0.6f);
        // Inverter draws from 12V battery with efficiency loss
        inverter_dc_input_w = load_220v_w / MULTIPLUS_EFFICIENCY_INVERTER;
        inverter_dc_input_a = inverter_dc_input_w / battery_voltage_v;
    }
    
    float total_load_w = load_12v_w + inverter_dc_input_w;
    float total_load_a = load_12v_a + inverter_dc_input_a;
    
    // ═══════════════════════════════════════════════════════════
    // STEP 5: Simulate AC MAINS CHARGER (Multiplus)
    // ═══════════════════════════════════════════════════════════
    bool ac_available = sim_ctx->ac_mains_available;
    float ac_input_voltage = 0.0f;
    float ac_input_current = 0.0f;
    float ac_charge_current_a = 0.0f;
    float ac_charge_power_w = 0.0f;
    
    if (ac_available && battery_soc < 98.0f) {
        ac_input_voltage = MULTIPLUS_AC_INPUT_VOLTAGE + random_float(-5.0f, 5.0f);
        
        // When AC available: can charge battery AND power 220V loads directly
        // Charge current depends on battery state
        float charge_current_max = MULTIPLUS_MAX_CHARGE_CURRENT_A;
        if (battery_voltage_v > 14.0f) {
            charge_current_max *= 0.4f; // Reduce in float stage
        }
        
        ac_charge_current_a = random_float(charge_current_max * 0.6f, charge_current_max);
        ac_charge_power_w = battery_voltage_v * ac_charge_current_a;
        
        // AC input power = charging + loads (with efficiency loss)
        float ac_input_power_w = (ac_charge_power_w / MULTIPLUS_EFFICIENCY_CHARGE);
        if (inverter_needed) {
            // 220V loads powered directly from AC (bypass battery)
            ac_input_power_w += load_220v_w;
            inverter_dc_input_w = 0.0f; // No DC draw when AC available
            inverter_dc_input_a = 0.0f;
            total_load_w = load_12v_w; // Only 12V loads from battery
            total_load_a = load_12v_a;
        }
        
        ac_input_current = ac_input_power_w / ac_input_voltage;
    }
    
    // Update inverter_charger state
    van_state->inverter_charger.enabled = inverter_needed || ac_available;
    van_state->inverter_charger.ac_input_voltage = ac_input_voltage;
    van_state->inverter_charger.ac_input_frequency = ac_available ? 50.0f : 0.0f;
    van_state->inverter_charger.ac_input_current = ac_input_current;
    van_state->inverter_charger.ac_input_power = ac_input_voltage * ac_input_current;
    
    // AC output (inverter mode)
    van_state->inverter_charger.ac_output_voltage = inverter_needed ? 230.0f : 0.0f;
    van_state->inverter_charger.ac_output_frequency = inverter_needed ? 50.0f : 0.0f;
    van_state->inverter_charger.ac_output_current = inverter_needed ? (load_220v_w / 230.0f) : 0.0f;
    van_state->inverter_charger.ac_output_power = load_220v_w;
    
    van_state->inverter_charger.battery_voltage = battery_voltage_v;
    van_state->inverter_charger.battery_current = ac_charge_current_a - inverter_dc_input_a;
    van_state->inverter_charger.inverter_temperature = 35.0f + (load_220v_w / 100.0f) + random_float(-2.0f, 2.0f);
    van_state->inverter_charger.charger_state = ac_available ? 
                                                 calculate_charge_state(battery_voltage_v) : 
                                                 CHARGE_STATE_OFF;
    van_state->inverter_charger.error_flags = 0;
    
    // ═══════════════════════════════════════════════════════════
    // STEP 6: Calculate BATTERY CURRENT (energy balance)
    // ═══════════════════════════════════════════════════════════
    
    // Total charging current
    float total_charge_current_a = solar_current_total_a +    // From solar
                                    alternator_output_current_a + // From alternator
                                    ac_charge_current_a;          // From AC mains
    
    // Net battery current (positive = charging, negative = discharging)
    float battery_net_current_a = total_charge_current_a - total_load_a;
    
    // Store all energy data in shared context for debugging
    sim_ctx->battery_net_current_a = battery_net_current_a;
    sim_ctx->ac_charger_power_w = ac_charge_power_w;
    sim_ctx->alternator_power_w = alternator_power_w;
    sim_ctx->load_12v_w = load_12v_w;
    sim_ctx->load_220v_w = load_220v_w;
    sim_ctx->inverter_loss_w = inverter_dc_input_w - load_220v_w; // Efficiency loss
    
    // DEBUG: Log to verify values
    if (sim_ctx->time_ticks % 500 == 1) {
        ESP_LOGI(TAG, "🔍 DEBUG: load_12v_w=%.1f, battery_net_current_a=%.2f, total_charge=%.2f, total_load=%.2f",
                 load_12v_w, battery_net_current_a, total_charge_current_a, total_load_a);
    }
    
    // ═══════════════════════════════════════════════════════════
    // STEP 7: Logging (every 10 seconds)
    // ═══════════════════════════════════════════════════════════
    if (sim_ctx->time_ticks % 500 == 0) {
        ESP_LOGI(TAG, "╔════════════════════════════════════════════════════════════╗");
        ESP_LOGI(TAG, "║                   ENERGY FLOW SUMMARY                      ║");
        ESP_LOGI(TAG, "╠════════════════════════════════════════════════════════════╣");
        
        // Sources
        ESP_LOGI(TAG, "║ ⚡ SOURCES:");
        ESP_LOGI(TAG, "║   🌞 Solar:      %6.1fW (%4.1fA)", solar_power_total_w, solar_current_total_a);
        ESP_LOGI(TAG, "║   🔌 AC Mains:   %6.1fW (%4.1fA) [%s]", 
                 ac_charge_power_w, ac_charge_current_a, ac_available ? "ON" : "OFF");
        ESP_LOGI(TAG, "║   🚗 Alternator: %6.1fW (%4.1fA) [%s]", 
                 alternator_power_w, alternator_output_current_a, engine_running ? "ON" : "OFF");
        ESP_LOGI(TAG, "║   📊 Total In:   %6.1fW (%4.1fA)", 
                 solar_power_total_w + ac_charge_power_w + alternator_power_w, total_charge_current_a);
        
        // Loads
        ESP_LOGI(TAG, "║ 🔋 LOADS:");
        ESP_LOGI(TAG, "║   💡 12V Devices: %6.1fW (%4.1fA)", load_12v_w, load_12v_a);
        ESP_LOGI(TAG, "║   🏠 220V Devices:%6.1fW (via %s)", 
                 load_220v_w, ac_available ? "AC direct" : "Inverter");
        ESP_LOGI(TAG, "║   🔌 Inverter DC: %6.1fW (%4.1fA)", inverter_dc_input_w, inverter_dc_input_a);
        ESP_LOGI(TAG, "║   📊 Total Out:  %6.1fW (%4.1fA)", total_load_w, total_load_a);
        
        // Battery
        ESP_LOGI(TAG, "║ 🔋 BATTERY:");
        ESP_LOGI(TAG, "║   Voltage:      %5.2fV", battery_voltage_v);
        ESP_LOGI(TAG, "║   Current:      %+5.1fA (%s)", 
                 battery_net_current_a, 
                 battery_net_current_a > 0.1f ? "CHARGING" : 
                 battery_net_current_a < -0.1f ? "DISCHARGING" : "IDLE");
        ESP_LOGI(TAG, "║   SOC:          %5.1f%%", battery_soc);
        ESP_LOGI(TAG, "╚════════════════════════════════════════════════════════════╝");
    }
    
#else
    // Real hardware implementation (TODO)
    ESP_LOGW(TAG, "Real hardware not yet implemented");
    van_state->alternator_charger.state = CHARGE_STATE_OFF;
    van_state->inverter_charger.enabled = false;
#endif
    
    return ESP_OK;
}
