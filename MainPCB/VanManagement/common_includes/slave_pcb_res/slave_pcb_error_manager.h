#ifndef MAIN_ERROR_MANAGER_H
#define MAIN_ERROR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"


#define MAX_ERROR_HISTORY 5
#define MAX_MODULE_NAME 32
#define MAX_DESCRIPTION 64


#include "../error_manager.h"

// Error codes
typedef enum {
    SLAVE_PCB_OK = 0,
    // Initialization errors (0x1XXX)
    SLAVE_PCB_ERR_INVALID_ARG = 0x1001,
    SLAVE_PCB_ERR_INIT_FAIL = 0x1002,
    SLAVE_PCB_ERR_MEMORY = 0x1003,
    
    // Communication errors (0x2XXX)
    SLAVE_PCB_ERR_COMM_FAIL = 0x2001,
    SLAVE_PCB_ERR_I2C_FAIL = 0x2002,
    SLAVE_PCB_ERR_SPI_FAIL = 0x2003,
    SLAVE_PCB_ERR_TIMEOUT = 0x2004,
    
    // Device errors (0x3XXX)
    SLAVE_PCB_ERR_DEVICE_NOT_FOUND = 0x3001,
    SLAVE_PCB_ERR_DEVICE_BUSY = 0x3002,
    SLAVE_PCB_ERR_DEVICE_FAULT = 0x3003,
    
    // State/Case errors (0x4XXX)
    SLAVE_PCB_ERR_STATE_INVALID = 0x4001,
    SLAVE_PCB_ERR_INCOMPATIBLE_CASE = 0x4002,
    SLAVE_PCB_ERR_CASE_TRANSITION = 0x4003,
    
    // Safety errors (0x5XXX)
    SLAVE_PCB_ERR_SAFETY_LIMIT = 0x5001,
    SLAVE_PCB_ERR_EMERGENCY_STOP = 0x5002,
    SLAVE_PCB_ERR_OVERCURRENT = 0x5003,
    SLAVE_PCB_ERR_SENSOR_RANGE = 0x5004
} slave_pcb_err_t;

// Structure d'événement d'erreur
typedef struct {
    uint32_t error_code;
    error_severity_t severity;
    error_category_t category;
    uint32_t timestamp;
    char module[MAX_MODULE_NAME];
    char description[MAX_DESCRIPTION];
    uint32_t data;
} slave_error_event_t;

// Statistiques des erreurs
typedef struct {
    uint32_t total_errors;
    uint32_t errors_by_severity[4];
    uint32_t errors_by_category[8];
    uint32_t last_error_timestamp;
    uint32_t last_error_code;
} slave_error_stats_t;

// État complet des erreurs
typedef struct {
    slave_error_stats_t error_stats;
    slave_error_event_t last_errors[MAX_ERROR_HISTORY];
} slave_error_state_t;

// Fonction d'affichage des erreurs
void print_slave_error_state(const slave_error_state_t* state);
void print_slave_error_event(const slave_error_event_t* event);
void print_slave_error_stats(const slave_error_stats_t* stats);

#endif // MAIN_ERROR_MANAGER_H