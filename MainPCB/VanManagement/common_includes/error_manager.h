#ifndef ERROR_MANAGER_H
#define ERROR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

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
    MAIN_PCB_OK = 0,
    // Initialization errors (0x1XXX)
    MAIN_PCB_ERR_INVALID_ARG = 0x1001,
    MAIN_PCB_ERR_INIT_FAIL = 0x1002,
    MAIN_PCB_ERR_MEMORY = 0x1003,
    
    // Communication errors (0x2XXX)
    MAIN_PCB_ERR_COMM_FAIL = 0x2001,
    MAIN_PCB_ERR_I2C_FAIL = 0x2002,
    MAIN_PCB_ERR_SPI_FAIL = 0x2003,
    MAIN_PCB_ERR_TIMEOUT = 0x2004,
    MAIN_PCB_ERR_ETH_DISCONNECTED = 0x2005,
    
    
    // Device errors (0x3XXX)
    MAIN_PCB_ERR_DEVICE_NOT_FOUND = 0x3001,
    MAIN_PCB_ERR_DEVICE_BUSY = 0x3002,
    MAIN_PCB_ERR_DEVICE_FAULT = 0x3003,
    
    // State/Case errors (0x4XXX)
    MAIN_PCB_ERR_STATE_INVALID = 0x4001,
    MAIN_PCB_ERR_INCOMPATIBLE_CASE = 0x4002,
    MAIN_PCB_ERR_CASE_TRANSITION = 0x4003,
    
    // Safety errors (0x5XXX)
    MAIN_PCB_ERR_SAFETY_LIMIT = 0x5001,
    MAIN_PCB_ERR_EMERGENCY_STOP = 0x5002,
    MAIN_PCB_ERR_OVERCURRENT = 0x5003,
    MAIN_PCB_ERR_SENSOR_RANGE = 0x5004
} main_pcb_err_t;

// Error event structure
typedef struct {
    main_pcb_err_t error_code;
    error_severity_t severity;
    error_category_t category;
    uint32_t timestamp;
    char module[32];
    char description[64];
    uint32_t data;            // Additional error-specific data
} error_event_t;

// Error statistics
typedef struct {
    uint32_t total_errors;
    uint32_t errors_by_severity[4];
    uint32_t errors_by_category[8];
    uint32_t last_error_timestamp;
    main_pcb_err_t last_error_code;
} error_stats_t;

// System error state for communication
typedef struct {
    error_stats_t error_stats;
    error_event_t last_errors[5];  // Keep last 5 errors for reporting
} main_error_state_t;

// Function declarations
void error_manager_init(void);
void error_manager_report(error_event_t* event);
const char* get_error_string(main_pcb_err_t error);
error_severity_t error_get_severity(main_pcb_err_t error);
error_category_t error_get_category(main_pcb_err_t error);
void error_get_stats(error_stats_t* stats);
bool error_is_critical(main_pcb_err_t error);
void error_clear_stats(void);
main_error_state_t* error_get_system_state(void);

// Helper macro to stringify the module name
#define ERROR_MODULE_NAME(name) #name

// Convenience macro for error reporting
// code: error code from main_pcb_err_t
// module_tag: the module's TAG variable or a string literal
// fmt: printf-style format string for the description
// ...: variable arguments for the format string and optional error data
#define REPORT_ERROR(code, module_tag, desc, error_data) do { \
    error_event_t event; \
    memset(&event, 0, sizeof(event)); \
    event.error_code = (code); \
    event.severity = error_get_severity(code); \
    event.category = error_get_category(code); \
    event.timestamp = esp_timer_get_time() / 1000; \
    event.data = (uint32_t)(error_data); \
    strncpy(event.module, (module_tag), sizeof(event.module) - 1); \
    event.module[sizeof(event.module) - 1] = '\0'; \
    strncpy(event.description, (desc), sizeof(event.description) - 1); \
    event.description[sizeof(event.description) - 1] = '\0'; \
    error_manager_report(&event); \
} while(0)

#endif // ERROR_MANAGER_H