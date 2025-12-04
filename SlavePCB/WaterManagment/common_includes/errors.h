/*#ifndef ERRORS_H
#define ERRORS_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

// Error severity levels
typedef enum {
    ERR_SEVERITY_INFO = 0,    // Informational, system continues normally
    ERR_SEVERITY_WARNING,     // Warning, might need attention
    ERR_SEVERITY_ERROR,       // Error, functionality impacted
    ERR_SEVERITY_CRITICAL     // Critical, system safety at risk
} error_severity_t;

// Error categories
typedef enum {
    ERR_CAT_NONE = 0,
    ERR_CAT_INIT = (1 << 0),
    ERR_CAT_COMM = (1 << 1),
    ERR_CAT_DEVICE = (1 << 2),
    ERR_CAT_SENSOR = (1 << 3),
    ERR_CAT_ACTUATOR = (1 << 4),
    ERR_CAT_SYSTEM = (1 << 5),
    ERR_CAT_CASE = (1 << 6),
    ERR_CAT_SAFETY = (1 << 7)
} error_category_t;

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

// Error event structure
typedef struct {
    slave_pcb_err_t error_code;
    error_severity_t severity;
    error_category_t category;
    uint32_t timestamp;
    char module[32];          // Changed from const char* to fixed size array
    char description[64];     // Changed from const char* to fixed size array
    uint32_t data;            // Additional error-specific data
} error_event_t;

#define MAX_ERROR_HISTORY 5

// Error statistics
typedef struct {
    uint32_t total_errors;
    uint32_t errors_by_severity[4];
    uint32_t errors_by_category[8];
    uint32_t last_error_timestamp;
    slave_pcb_err_t last_error_code;
} error_stats_t;

// Complete error state structure
typedef struct {
    error_stats_t error_stats;
    error_event_t last_errors[MAX_ERROR_HISTORY];  // Circular buffer of the last errors
} system_error_state_t;

// Function declarations
void error_manager_init(void);
void error_manager_report(error_event_t* event);
const char* error_get_description(slave_pcb_err_t error);
error_severity_t error_get_severity(slave_pcb_err_t error);
error_category_t error_get_category(slave_pcb_err_t error);
void error_get_stats(error_stats_t* stats);
system_error_state_t* error_get_system_state(void);
bool error_is_critical(slave_pcb_err_t error);
void error_clear_stats(void);

// Convenience macro for error reporting
#define REPORT_ERROR(code, module, desc, data) do { \
    error_event_t event = { \
        .error_code = (code), \
        .severity = error_get_severity(code), \
        .category = error_get_category(code), \
        .timestamp = esp_timer_get_time() / 1000, \
        .data = (data) \
    }; \
    strncpy(event.module, (module), sizeof(event.module) - 1); \
    event.module[sizeof(event.module) - 1] = '\0'; \
    strncpy(event.description, (desc), sizeof(event.description) - 1); \
    event.description[sizeof(event.description) - 1] = '\0'; \
    error_manager_report(&event); \
} while(0)

#endif // ERRORS_H
*/