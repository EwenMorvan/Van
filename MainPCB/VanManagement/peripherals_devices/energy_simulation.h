#ifndef ENERGY_SIMULATION_H
#define ENERGY_SIMULATION_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration - actual implementation only compiled when ENABLE_ENERGY_SIMULATION is defined

/**
 * @brief Shared simulation context for all energy managers
 * 
 * This ensures all simulations (battery, mppt, inverter/chargers) are synchronized
 * and energy flows are coherent across the system.
 */
typedef struct {
    uint32_t time_ticks;           // Shared simulation time counter
    float battery_net_current_a;   // Calculated net battery current (for battery simulation)
    float battery_voltage_v;       // Current battery voltage
    float battery_soc_percent;     // Current battery state of charge
    bool ac_mains_available;       // AC mains status
    bool engine_running;           // Alternator/engine status
    float day_cycle;               // Solar day cycle (0.0=night, 1.0=noon)
    
    // Energy sources (in Watts)
    float solar_power_w;           // Total solar generation power
    float solar_current_a;         // Total solar generation current
    float ac_charger_power_w;      // AC mains charger power
    float alternator_power_w;      // Alternator charger power
    
    // Energy loads (in Watts, always positive)
    float load_12v_w;              // 12V DC loads
    float load_220v_w;             // 220V AC loads (via inverter)
    float inverter_loss_w;         // Inverter efficiency losses
} energy_simulation_context_t;

/**
 * @brief Initialize shared simulation context
 */
void energy_simulation_init(void);

/**
 * @brief Get shared simulation context (thread-safe)
 */
energy_simulation_context_t* energy_simulation_get_context(void);

/**
 * @brief Update simulation time (called once per update cycle)
 */
void energy_simulation_update_time(void);

/**
 * @brief Print detailed energy simulation summary for debugging
 */
void energy_simulation_print_summary(void);

#endif // ENERGY_SIMULATION_H
