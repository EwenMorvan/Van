#include "error_manager.h"
#include <string.h>

#define ERROR_QUEUE_SIZE 32
#define MAX_STORED_ERRORS 5

static QueueHandle_t error_queue = NULL;
static error_stats_t error_statistics = {0};
static main_error_state_t system_error_state = {0};
static const char *TAG = "ERROR_MGR";

static void update_error_history(const error_event_t* event) {
    // Shift existing errors
    for (int i = MAX_STORED_ERRORS - 1; i > 0; i--) {
        system_error_state.last_errors[i] = system_error_state.last_errors[i-1];
    }
    // Add new error at the start
    system_error_state.last_errors[0] = *event;
}

void error_manager_init(void) {
    error_queue = xQueueCreate(ERROR_QUEUE_SIZE, sizeof(error_event_t));
    if (!error_queue) {
        ESP_LOGE(TAG, "Failed to create error queue");
    }
    memset(&error_statistics, 0, sizeof(error_statistics));
    memset(&system_error_state, 0, sizeof(system_error_state));
}

void error_manager_report(error_event_t* event) {
    if (!event) return;
    
    // Update statistics
    error_statistics.total_errors++;
    error_statistics.errors_by_severity[event->severity]++;
    error_statistics.errors_by_category[__builtin_ffs(event->category) - 1]++;
    error_statistics.last_error_timestamp = event->timestamp;
    error_statistics.last_error_code = event->error_code;
    
    // Update system error state
    system_error_state.error_stats = error_statistics;
    update_error_history(event);
    
    // Log the error with colors
    const char* severity_str[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
    const char* severity_color[] = {
        "\033[0;32m",  // Green for INFO
        "\033[0;33m",  // Yellow for WARNING
        "\033[0;31m",  // Red for ERROR
        "\033[1;31m"   // Bright Red for CRITICAL
    };
    const char* reset_color = "\033[0m";
    
    ESP_LOG_LEVEL(
        event->severity == ERR_SEVERITY_INFO ? ESP_LOG_INFO :
        event->severity == ERR_SEVERITY_WARNING ? ESP_LOG_WARN :
        ESP_LOG_ERROR,
        TAG, "%s[%s]%s [%s] %s: %s (0x%X)",
        severity_color[event->severity],
        severity_str[event->severity],
        reset_color,
        event->module,
        get_error_string(event->error_code),
        event->description,
        event->data);
    
    // Queue the error for processing
    if (error_queue) {
        if (xQueueSend(error_queue, event, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Error queue full, dropping error");
        }
    }
    
    // Take immediate action for critical errors
    if (event->severity == ERR_SEVERITY_CRITICAL) {
        // TODO: Implement system-wide critical error handling
        ESP_LOGE(TAG, "CRITICAL ERROR DETECTED - Initiating safety protocol");
        // Example: Could trigger system reset, emergency stop, etc.
    }
}

const char* get_error_string(main_pcb_err_t error) {
    switch (error) {
        case MAIN_PCB_OK: return "Success";
        // Initialization errors
        case MAIN_PCB_ERR_INVALID_ARG: return "Invalid argument";
        case MAIN_PCB_ERR_INIT_FAIL: return "Initialization failed";
        case MAIN_PCB_ERR_MEMORY: return "Memory allocation failed";
        
        // Communication errors
        case MAIN_PCB_ERR_COMM_FAIL: return "Communication failure";
        case MAIN_PCB_ERR_I2C_FAIL: return "I2C communication failed";
        case MAIN_PCB_ERR_SPI_FAIL: return "SPI communication failed";
        case MAIN_PCB_ERR_TIMEOUT: return "Operation timeout";
        
        // Device errors
        case MAIN_PCB_ERR_DEVICE_NOT_FOUND: return "Device not found";
        case MAIN_PCB_ERR_DEVICE_BUSY: return "Device busy";
        case MAIN_PCB_ERR_DEVICE_FAULT: return "Device fault detected";
        
        // State/Case errors
        case MAIN_PCB_ERR_STATE_INVALID: return "Invalid state";
        case MAIN_PCB_ERR_INCOMPATIBLE_CASE: return "Incompatible case";
        case MAIN_PCB_ERR_CASE_TRANSITION: return "Case transition failed";
        
        // Safety errors
        case MAIN_PCB_ERR_SAFETY_LIMIT: return "Safety limit exceeded";
        case MAIN_PCB_ERR_EMERGENCY_STOP: return "Emergency stop triggered";
        case MAIN_PCB_ERR_OVERCURRENT: return "Overcurrent detected";
        case MAIN_PCB_ERR_SENSOR_RANGE: return "Sensor value out of range";
        
        default: return "Unknown error";
    }
}

error_severity_t error_get_severity(main_pcb_err_t error) {
    if (error == MAIN_PCB_OK) return ERR_SEVERITY_INFO;
    
    // Determine severity based on error code range
    uint16_t error_class = error & 0xF000;
    switch (error_class) {
        case 0x1000: // Initialization errors
            return ERR_SEVERITY_ERROR;
        case 0x2000: // Communication errors
            return ERR_SEVERITY_WARNING;
        case 0x3000: // Device errors
            return ERR_SEVERITY_ERROR;
        case 0x4000: // State/Case errors
            return ERR_SEVERITY_WARNING;
        case 0x5000: // Safety errors
            return ERR_SEVERITY_CRITICAL;
        default:
            return ERR_SEVERITY_ERROR;
    }
}

error_category_t error_get_category(main_pcb_err_t error) {
    if (error == MAIN_PCB_OK) return ERR_CAT_NONE;
    
    uint16_t error_class = error & 0xF000;
    switch (error_class) {
        case 0x1000: return ERR_CAT_INIT;
        case 0x2000: return ERR_CAT_COMM;
        case 0x3000: return ERR_CAT_DEVICE;
        case 0x4000: return ERR_CAT_CASE;
        case 0x5000: return ERR_CAT_SAFETY;
        default: return ERR_CAT_SYSTEM;
    }
}

void error_get_stats(error_stats_t* stats) {
    if (stats) {
        memcpy(stats, &error_statistics, sizeof(error_statistics));
    }
}

bool error_is_critical(main_pcb_err_t error) {
    return error_get_severity(error) == ERR_SEVERITY_CRITICAL;
}

void error_clear_stats(void) {
    memset(&error_statistics, 0, sizeof(error_statistics));
    memset(&system_error_state, 0, sizeof(system_error_state));
}
main_error_state_t* error_get_system_state(void) {
    return &system_error_state;
}