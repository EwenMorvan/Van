#include "slave_pcb.h"

static const char *TAG = "ERROR_MGR";

// Error counters for monitoring system health
typedef struct {
    uint32_t i2c_errors;
    uint32_t spi_errors;
    uint32_t gpio_errors;
    uint32_t timeout_errors;
    uint32_t memory_errors;
    uint32_t communication_errors;
    uint32_t incompatible_case_errors;
    uint32_t device_errors;
    uint32_t total_errors;
} error_counters_t;

static error_counters_t error_counters = {0};

// System health status
typedef struct {
    bool system_healthy;
    uint32_t last_health_check;
    uint32_t uptime_seconds;
    uint32_t free_heap_size;
    uint32_t min_free_heap_size;
} system_health_t;

static system_health_t system_health = {0};

/**
 * @brief Log and count system errors
 */
void log_system_error(slave_pcb_err_t error_code, const char* component, const char* description) {
    error_counters.total_errors++;
    
    // Increment specific error counter
    switch (error_code) {
        case SLAVE_PCB_ERR_I2C_FAIL:
            error_counters.i2c_errors++;
            break;
        case SLAVE_PCB_ERR_SPI_FAIL:
            error_counters.spi_errors++;
            break;
        case SLAVE_PCB_ERR_TIMEOUT:
            error_counters.timeout_errors++;
            break;
        case SLAVE_PCB_ERR_MEMORY:
            error_counters.memory_errors++;
            break;
        case SLAVE_PCB_ERR_COMM_FAIL:
            error_counters.communication_errors++;
            break;
        case SLAVE_PCB_ERR_INCOMPATIBLE_CASE:
            error_counters.incompatible_case_errors++;
            break;
        case SLAVE_PCB_ERR_DEVICE_NOT_FOUND:
            error_counters.device_errors++;
            break;
        default:
            break;
    }

    ESP_LOGE(TAG, "[%s] Error %d: %s", component, error_code, description);
    
    // Send error to communication manager if available
    if (comm_queue != NULL) {
        comm_msg_t error_msg = {
            .type = MSG_ERROR,
            .timestamp = esp_timer_get_time() / 1000,
            .data.error_data = {
                .error_code = error_code,
            }
        };
        
        snprintf(error_msg.data.error_data.description, 
                sizeof(error_msg.data.error_data.description),
                "[%s] %s", component, description);
        
        xQueueSend(comm_queue, &error_msg, 0);
    }
}

/**
 * @brief Check system health and update status
 */
void check_system_health(void) {
    uint32_t now = esp_timer_get_time() / 1000000; // seconds
    system_health.uptime_seconds = now;
    
    // Get heap information
    system_health.free_heap_size = esp_get_free_heap_size();
    system_health.min_free_heap_size = esp_get_minimum_free_heap_size();
    
    // Check if system is healthy based on various criteria
    bool was_healthy = system_health.system_healthy;
    
    system_health.system_healthy = true;
    
    // Check heap usage (system unhealthy if less than 10KB free)
    if (system_health.free_heap_size < 10240) {
        system_health.system_healthy = false;
        ESP_LOGW(TAG, "Low heap warning: %lu bytes free", system_health.free_heap_size);
    }
    
    // Check error rate (unhealthy if more than 100 errors in last minute)
    static uint32_t last_error_count = 0;
    static uint32_t last_error_check = 0;
    
    if (now - last_error_check >= 60) { // Check every minute
        uint32_t error_rate = error_counters.total_errors - last_error_count;
        if (error_rate > 100) {
            system_health.system_healthy = false;
            ESP_LOGW(TAG, "High error rate: %lu errors/minute", error_rate);
        }
        last_error_count = error_counters.total_errors;
        last_error_check = now;
    }
    
    // Log health status change
    if (was_healthy != system_health.system_healthy) {
        if (system_health.system_healthy) {
            ESP_LOGI(TAG, "System health restored");
        } else {
            ESP_LOGW(TAG, "System health degraded");
        }
    }
    
    system_health.last_health_check = now;
}

/**
 * @brief Get error statistics
 */
void get_error_statistics(error_counters_t* stats) {
    if (stats) {
        memcpy(stats, &error_counters, sizeof(error_counters_t));
    }
}

/**
 * @brief Get system health status
 */
void get_system_health(system_health_t* health) {
    if (health) {
        memcpy(health, &system_health, sizeof(system_health_t));
    }
}

/**
 * @brief Reset error counters
 */
void reset_error_counters(void) {
    memset(&error_counters, 0, sizeof(error_counters_t));
    ESP_LOGI(TAG, "Error counters reset");
}

/**
 * @brief Print system status report
 */
void print_system_status(void) {
    check_system_health();
    
    ESP_LOGI(TAG, "=== System Status Report ===");
    ESP_LOGI(TAG, "Uptime: %lu seconds", system_health.uptime_seconds);
    ESP_LOGI(TAG, "System Health: %s", system_health.system_healthy ? "HEALTHY" : "DEGRADED");
    ESP_LOGI(TAG, "Free Heap: %lu bytes (min: %lu bytes)", 
             system_health.free_heap_size, system_health.min_free_heap_size);
    ESP_LOGI(TAG, "Total Errors: %lu", error_counters.total_errors);
    ESP_LOGI(TAG, "  I2C Errors: %lu", error_counters.i2c_errors);
    ESP_LOGI(TAG, "  SPI Errors: %lu", error_counters.spi_errors);
    ESP_LOGI(TAG, "  Timeout Errors: %lu", error_counters.timeout_errors);
    ESP_LOGI(TAG, "  Memory Errors: %lu", error_counters.memory_errors);
    ESP_LOGI(TAG, "  Communication Errors: %lu", error_counters.communication_errors);
    ESP_LOGI(TAG, "  Incompatible Case Errors: %lu", error_counters.incompatible_case_errors);
    ESP_LOGI(TAG, "  Device Errors: %lu", error_counters.device_errors);
    ESP_LOGI(TAG, "============================");
}
