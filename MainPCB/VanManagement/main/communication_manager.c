#include "communication_manager.h"
#include "w5500_ethernet.h"
#include "protocol.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "COMM_MGR";
static QueueHandle_t comm_queue;
static TaskHandle_t comm_task_handle;

esp_err_t communication_manager_init(void) {
    comm_queue = xQueueCreate(COMM_QUEUE_SIZE, sizeof(comm_message_t));
    if (comm_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create communication queue");
        return ESP_FAIL;
    }
    
    // Initialize W5500 Ethernet - DISABLED for BLE communication
    // esp_err_t ret = w5500_ethernet_init();
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to initialize W5500 Ethernet");
    //     return ret;
    // }
    
    // Create communication task
    BaseType_t result = xTaskCreate(
        communication_manager_task,
        "comm_manager",
        4096,
        NULL,
        5,
        &comm_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create communication task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Communication manager initialized");
    return ESP_OK;
}

esp_err_t comm_send_message(comm_msg_type_t type, void *data, size_t size) {
    comm_message_t msg = {
        .type = type,
        .data = malloc(size),
        .data_size = size
    };
    
    if (msg.data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for message");
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(msg.data, data, size);
    
    if (xQueueSend(comm_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        free(msg.data);
        ESP_LOGW(TAG, "Failed to send message to queue");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t comm_send_command(van_command_t *cmd) {
    return comm_send_message(COMM_MSG_COMMAND, cmd, sizeof(van_command_t));
}

QueueHandle_t comm_get_queue(void) {
    return comm_queue;
}

void communication_manager_task(void *parameters) {
    comm_message_t msg;
    van_state_t *current_state;
    
    ESP_LOGI(TAG, "Communication manager task started");
    
    while (1) {
        // Process incoming messages
        if (xQueueReceive(comm_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (msg.type) {
                case COMM_MSG_SENSOR_UPDATE:
                    // Update sensor data in global state
                    current_state = protocol_get_state();
                    if (current_state) {
                        memcpy(&current_state->sensors, msg.data, sizeof(current_state->sensors));
                        protocol_update_state(current_state);
                    }
                    break;
                    
                case COMM_MSG_MPPT_UPDATE:
                    // Update MPPT data in global state
                    current_state = protocol_get_state();
                    if (current_state) {
                        memcpy(&current_state->mppt, msg.data, sizeof(current_state->mppt));
                        protocol_update_state(current_state);
                    }
                    break;
                    
                case COMM_MSG_HEATER_UPDATE:
                    // Update heater data in global state
                    current_state = protocol_get_state();
                    if (current_state) {
                        memcpy(&current_state->heater, msg.data, sizeof(current_state->heater));
                        protocol_update_state(current_state);
                    }
                    break;
                    
                case COMM_MSG_FAN_UPDATE:
                    // Update fan data in global state
                    current_state = protocol_get_state();
                    if (current_state) {
                        memcpy(&current_state->fans, msg.data, sizeof(current_state->fans));
                        protocol_update_state(current_state);
                    }
                    break;
                    
                case COMM_MSG_LED_UPDATE:
                    // Update LED data in global state
                    current_state = protocol_get_state();
                    if (current_state) {
                        memcpy(&current_state->leds, msg.data, sizeof(current_state->leds));
                        protocol_update_state(current_state);
                    }
                    break;
                    
                case COMM_MSG_COMMAND:
                    // Process incoming command
                    protocol_process_command((van_command_t*)msg.data);
                    break;
                    
                case COMM_MSG_ERROR:
                    // Handle error message
                    protocol_set_error(*(uint32_t*)msg.data);
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown message type: %d", msg.type);
                    break;
            }
            
            // Free allocated memory
            if (msg.data) {
                free(msg.data);
            }
        }
        
        // Send periodic state updates to SlavePCB - DISABLED for BLE communication
        // current_state = protocol_get_state();
        // if (current_state) {
        //     w5500_send_state(current_state);
        // }
        
        // Check for incoming data from SlavePCB - DISABLED for BLE communication
        // van_command_t incoming_cmd;
        // if (w5500_receive_command(&incoming_cmd) == ESP_OK) {
        //     protocol_process_command(&incoming_cmd);
        // }
    }
}
