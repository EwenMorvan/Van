#include "mppt_manager.h"
#include "communication_manager.h"
#include "uart_multiplexer.h"
#include "gpio_pinout.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>



static const char *TAG = "MPPT_MGR";
static TaskHandle_t mppt_task_handle;

typedef struct {
    float solar_power;
    float battery_voltage;
    float battery_current;
    int8_t temperature;
    uint8_t state;
    bool data_valid;
} mppt_data_t;

static mppt_data_t mppt_100_50_data;
static mppt_data_t mppt_70_15_data;

esp_err_t mppt_manager_init(void) {
    // Initialize MPPT data structures
    memset(&mppt_100_50_data, 0, sizeof(mppt_data_t));
    memset(&mppt_70_15_data, 0, sizeof(mppt_data_t));
    
    // Create MPPT task on CPU1 for better load balancing
    BaseType_t result = xTaskCreatePinnedToCore(
        mppt_manager_task,
        "mppt_manager",
        4096,
        NULL,
        3,
        &mppt_task_handle,
        1  // CPU1
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MPPT task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "MPPT manager initialized with UART multiplexing");
    return ESP_OK;
}

static void parse_ve_direct_frame(const char *frame, mppt_data_t *data) {
    char *line = strtok((char*)frame, "\n\r");
    
    while (line != NULL) {
        if (strncmp(line, "V\t", 2) == 0) {
            // Battery voltage in mV
            data->battery_voltage = atof(line + 2) / 1000.0f;
        } else if (strncmp(line, "I\t", 2) == 0) {
            // Battery current in mA
            data->battery_current = atof(line + 2) / 1000.0f;
        } else if (strncmp(line, "PPV\t", 4) == 0) {
            // Panel power in W
            data->solar_power = atof(line + 4);
        } else if (strncmp(line, "CS\t", 3) == 0) {
            // Charger state
            data->state = (uint8_t)atoi(line + 3);
        } else if (strncmp(line, "T\t", 2) == 0) {
            // Temperature in Celsius
            data->temperature = (int8_t)atoi(line + 2);
        } else if (strncmp(line, "Checksum\t", 9) == 0) {
            // End of frame
            data->data_valid = true;
            break;
        }
        
        line = strtok(NULL, "\n\r");
    }
}

static void read_mppt_data(mppt_device_t device, mppt_data_t *data) {
    static char buffer[VE_DIRECT_FRAME_SIZE];
    static int buffer_pos = 0;
    
    // Switch to the desired MPPT device
    ESP_LOGD(TAG, "Switching to MPPT device %d", device);
    if (uart_mux_switch_mppt(device) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to switch to MPPT device %d", device);
        return;
    }
    
    uint8_t uart_data[64];
    int len = uart_mux_read_mppt(uart_data, sizeof(uart_data), 1000);
    
    ESP_LOGD(TAG, "MPPT %d: Read %d bytes from UART", device, len);
    
    if (len > 0) {
        // Log raw data for debugging
        ESP_LOG_BUFFER_HEXDUMP(TAG, uart_data, len, ESP_LOG_DEBUG);
        
        for (int i = 0; i < len; i++) {
            char c = uart_data[i];
            
            if (buffer_pos < VE_DIRECT_FRAME_SIZE - 1) {
                buffer[buffer_pos++] = c;
            }
            
            // Check for frame start
            if (c == ':' && buffer_pos > 1) {
                // Found start of new frame, reset buffer
                buffer[0] = ':';
                buffer_pos = 1;
            }
            
            // Check for checksum line (end of frame)
            if (strstr(buffer, "Checksum") != NULL) {
                buffer[buffer_pos] = '\0';
                parse_ve_direct_frame(buffer, data);
                buffer_pos = 0;
                ESP_LOGD(TAG, "MPPT %d data: Power=%.1fW, Voltage=%.2fV, Current=%.2fA, Temp=%d°C", 
                         device, data->solar_power, data->battery_voltage, data->battery_current, data->temperature);
                break;
            }
        }
    } else {
        ESP_LOGW(TAG, "No data received from MPPT %d", device);
        data->data_valid = false;
    }
}

void mppt_manager_task(void *parameters) {
    ESP_LOGI(TAG, "MPPT manager task started");
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        // Read data from both MPPTs using multiplexer
        read_mppt_data(MPPT_100_50, &mppt_100_50_data);
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between reads
        read_mppt_data(MPPT_70_15, &mppt_70_15_data);
        
        // Check for communication errors
        static uint32_t last_100_50_update = 0;
        static uint32_t last_70_15_update = 0;
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        if (mppt_100_50_data.data_valid) {
            last_100_50_update = current_time;
            mppt_100_50_data.data_valid = false; // Reset flag
        }
        
        if (mppt_70_15_data.data_valid) {
            last_70_15_update = current_time;
            mppt_70_15_data.data_valid = false; // Reset flag
        }
        
        // Check for communication timeout (30 seconds)
        if (current_time - last_100_50_update > 30000) {
            uint32_t error = ERROR_MPPT_COMM;
            comm_send_message(COMM_MSG_ERROR, &error, sizeof(uint32_t));
        }
        
        if (current_time - last_70_15_update > 30000) {
            uint32_t error = ERROR_MPPT_COMM;
            comm_send_message(COMM_MSG_ERROR, &error, sizeof(uint32_t));
        }
        
        // Prepare MPPT data for communication manager
        struct {
            float solar_power_100_50;
            float battery_voltage_100_50;
            float battery_current_100_50;
            int8_t temperature_100_50;
            uint8_t state_100_50;
            
            float solar_power_70_15;
            float battery_voltage_70_15;
            float battery_current_70_15;
            int8_t temperature_70_15;
            uint8_t state_70_15;
        } mppt_data = {
            .solar_power_100_50 = mppt_100_50_data.solar_power,
            .battery_voltage_100_50 = mppt_100_50_data.battery_voltage,
            .battery_current_100_50 = mppt_100_50_data.battery_current,
            .temperature_100_50 = mppt_100_50_data.temperature,
            .state_100_50 = mppt_100_50_data.state,
            
            .solar_power_70_15 = mppt_70_15_data.solar_power,
            .battery_voltage_70_15 = mppt_70_15_data.battery_voltage,
            .battery_current_70_15 = mppt_70_15_data.battery_current,
            .temperature_70_15 = mppt_70_15_data.temperature,
            .state_70_15 = mppt_70_15_data.state
        };
        
        // Send MPPT data to communication manager
        comm_send_message(COMM_MSG_MPPT_UPDATE, &mppt_data, sizeof(mppt_data));
        
        ESP_LOGD(TAG, "MPPT 100|50: %.1fW, %.2fV, %.2fA, %d°C, State:%d", 
                mppt_100_50_data.solar_power, mppt_100_50_data.battery_voltage, 
                mppt_100_50_data.battery_current, mppt_100_50_data.temperature, mppt_100_50_data.state);
        
        ESP_LOGD(TAG, "MPPT 70|15: %.1fW, %.2fV, %.2fA, %d°C, State:%d", 
                mppt_70_15_data.solar_power, mppt_70_15_data.battery_voltage, 
                mppt_70_15_data.battery_current, mppt_70_15_data.temperature, mppt_70_15_data.state);
        
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(MPPT_UPDATE_INTERVAL_MS));
    }
}
