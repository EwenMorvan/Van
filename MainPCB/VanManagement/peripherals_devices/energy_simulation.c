/**
 * @file energy_simulation.c
 * @brief Shared energy simulation context for all managers
 * 
 * Provides synchronized time and state for realistic energy flow simulation
 * Only compiled when ENABLE_ENERGY_SIMULATION is defined
 */

#include "energy_simulation.h"
#include "../common_includes/simulation_config.h"

#ifdef ENABLE_ENERGY_SIMULATION

#include <string.h>
#include <math.h>
#include "esp_log.h"

static const char *TAG = "ENERGY_SIM";

// Global shared context
static energy_simulation_context_t g_sim_context = {0};

void energy_simulation_init(void) {
    memset(&g_sim_context, 0, sizeof(energy_simulation_context_t));
    g_sim_context.battery_voltage_v = 12.8f;
    g_sim_context.battery_soc_percent = 65.0f;
}

energy_simulation_context_t* energy_simulation_get_context(void) {
    return &g_sim_context;
}

void energy_simulation_update_time(void) {
    g_sim_context.time_ticks++;
    
    // Update day cycle (0.0 = night, 1.0 = noon)
    g_sim_context.day_cycle = fmaxf(0.0f, sinf(g_sim_context.time_ticks * 0.01f));
    
    // Update AC mains availability (blocks of ~5 minutes)
    uint32_t ac_block = g_sim_context.time_ticks / 300;
    g_sim_context.ac_mains_available = (ac_block % 5) == 0;
    
    // Update engine running status (blocks of ~3 minutes)
    uint32_t engine_block = g_sim_context.time_ticks / 180;
    g_sim_context.engine_running = (engine_block % 7) == 0;
}

void energy_simulation_print_summary(void) {
    ESP_LOGI(TAG, "╔════════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║          ENERGY SIMULATION COHERENCE CHECK                     ║");
    ESP_LOGI(TAG, "╠════════════════════════════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║ 🕐 TIME: tick=%lu (%.1fs) | Day: %.1f%% | AC:%s Eng:%s        ║",
             g_sim_context.time_ticks, 
             g_sim_context.time_ticks * 0.02f,
             g_sim_context.day_cycle * 100.0f,
             g_sim_context.ac_mains_available ? "✓" : "✗",
             g_sim_context.engine_running ? "✓" : "✗");
    ESP_LOGI(TAG, "╠════════════════════════════════════════════════════════════════╣");
    
    // Calculate all powers
    float battery_power_w = g_sim_context.battery_net_current_a * g_sim_context.battery_voltage_v;
    float total_sources_w = g_sim_context.solar_power_w + 
                           g_sim_context.ac_charger_power_w + 
                           g_sim_context.alternator_power_w;
    float total_loads_w = g_sim_context.load_12v_w + 
                         g_sim_context.load_220v_w + 
                         g_sim_context.inverter_loss_w;
    
    ESP_LOGI(TAG, "║ ⚡ SOURCES (generation):                                       ║");
    ESP_LOGI(TAG, "║   🌞 Solar:         +%7.1f W                                 ║", g_sim_context.solar_power_w);
    ESP_LOGI(TAG, "║   🔌 AC Charger:    +%7.1f W                                 ║", g_sim_context.ac_charger_power_w);
    ESP_LOGI(TAG, "║   🚗 Alternator:    +%7.1f W                                 ║", g_sim_context.alternator_power_w);
    ESP_LOGI(TAG, "║   ───────────────────────────                                  ║");
    ESP_LOGI(TAG, "║   📊 TOTAL IN:      +%7.1f W                                 ║", total_sources_w);
    ESP_LOGI(TAG, "║                                                                ║");
    ESP_LOGI(TAG, "║ 🔋 BATTERY (storage):                                          ║");
    ESP_LOGI(TAG, "║   Voltage:     %6.2f V  |  SOC: %5.1f%%                      ║", 
             g_sim_context.battery_voltage_v, g_sim_context.battery_soc_percent);
    ESP_LOGI(TAG, "║   Power:       %+7.1f W  (%s)                    ║",
             battery_power_w,
             battery_power_w > 1.0f ? "CHARGING ⬆" : 
             battery_power_w < -1.0f ? "DISCHARGING ⬇" : "IDLE ─");
    ESP_LOGI(TAG, "║                                                                ║");
    ESP_LOGI(TAG, "║ 💡 LOADS (consumption):                                        ║");
    ESP_LOGI(TAG, "║   💡 12V devices:   -%7.1f W                                 ║", g_sim_context.load_12v_w);
    ESP_LOGI(TAG, "║   🏠 220V devices:  -%7.1f W                                 ║", g_sim_context.load_220v_w);
    ESP_LOGI(TAG, "║   🔥 Inverter loss: -%7.1f W                                 ║", g_sim_context.inverter_loss_w);
    ESP_LOGI(TAG, "║   ───────────────────────────                                  ║");
    ESP_LOGI(TAG, "║   📊 TOTAL OUT:     -%7.1f W                                 ║", total_loads_w);
    ESP_LOGI(TAG, "╠════════════════════════════════════════════════════════════════╣");
    
    // Energy conservation check
    float energy_balance = total_sources_w - total_loads_w;
    float energy_error = fabsf(energy_balance - battery_power_w);
    bool energy_conserved = energy_error < 5.0f; // 5W tolerance
    
    ESP_LOGI(TAG, "║ 🔬 ENERGY CONSERVATION:                                        ║");
    ESP_LOGI(TAG, "║   Sources - Loads = %+7.1f W                                 ║", energy_balance);
    ESP_LOGI(TAG, "║   Battery Power   = %+7.1f W                                 ║", battery_power_w);
    ESP_LOGI(TAG, "║   Error           = %7.1f W                                  ║", energy_error);
    ESP_LOGI(TAG, "║   Status:           %s                                  ║",
             energy_conserved ? "✅ CONSERVED" : "❌ ERROR");
    
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════════════════╝");
}

#endif // ENABLE_ENERGY_SIMULATION
