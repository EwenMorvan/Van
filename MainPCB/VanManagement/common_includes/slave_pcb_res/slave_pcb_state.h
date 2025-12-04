#ifndef SLAVE_PCB_STATE_H
#define SLAVE_PCB_STATE_H
#include <stdint.h>
#include <stdbool.h>
#include "../error_manager.h"
#include "slave_pcb_error_manager.h"
#include "slave_pcb_cases.h"

// ============================================================================
// STATE STRUCTURES
// ============================================================================


// Slave PCB system health status
typedef struct {
    bool system_healthy;
    uint32_t last_health_check;
    uint32_t uptime_seconds;
    uint32_t free_heap_size;
    uint32_t min_free_heap_size;
} slave_health_t;

// Hood state
typedef enum {
    HOOD_OFF = 0,
    HOOD_ON
} hood_state_t;

// Water tank data
typedef struct{
    float level_percentage;
    float weight_kg;
    float volume_liters;
} water_tank_data_t;

// Water tanks levels
typedef struct {
    water_tank_data_t tank_a;
    water_tank_data_t tank_b;
    water_tank_data_t tank_c;
    water_tank_data_t tank_d;
    water_tank_data_t tank_e;
} water_tanks_levels_t;

typedef struct {
    uint32_t timestamp;
    system_case_t current_case;
    hood_state_t hood_state;
    water_tanks_levels_t tanks_levels;
    slave_error_state_t error_state;
    slave_health_t system_health;
} slave_pcb_state_t;

#endif // SLAVE_PCB_STATE_H