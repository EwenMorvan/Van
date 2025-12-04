#include "slave_main_communication_manager.h"


static const char *TAG = "COMM_MGR";

// Protocol message structures for internal use
typedef struct __attribute__((packed)) {
    comm_msg_header_t header;
    comm_cmd_t command;
    uint8_t parameters[32];
} comm_command_msg_t;

typedef struct __attribute__((packed)) {
    comm_msg_header_t header;
    uint16_t command_sequence;
    slave_pcb_err_t status;
} comm_response_msg_t;

typedef struct __attribute__((packed)) {
    comm_msg_header_t header;
    slave_pcb_state_t state;
} comm_state_msg_t;

// Global variables
static uint16_t sequence_counter = 0;
static slave_pcb_state_t last_received_state = {0};
static SemaphoreHandle_t state_mutex = NULL;

// Process received command
static esp_err_t process_command(const comm_command_msg_t* cmd_msg) {
    switch (cmd_msg->command) {
        case CMD_SET_HOOD_ON:
            ESP_LOGI(TAG, "Processing command: SET_HOOD_ON");
            // Add logic to turn hood on
            hood_set_state(HOOD_ON);
            break;
        case CMD_SET_HOOD_OFF:
            ESP_LOGI(TAG, "Processing command: SET_HOOD_OFF");
            // Add logic to turn hood off
            hood_set_state(HOOD_OFF);
            break;
        default:
            ESP_LOGE(TAG, "Unknown command: %d", cmd_msg->command);
            return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

// Handle received state message
static void handle_state_message(const slave_pcb_state_t* state) {
    ESP_LOGD(TAG, "\n\nReceived state update:");
    ESP_LOGD(TAG, "  Timestamp: %lu", state->timestamp);
    ESP_LOGD(TAG, "  Current case: %d", state->current_case);
    ESP_LOGD(TAG, "  Hood state: %d", state->hood_state);

    ESP_LOGD(TAG, "  Tank levels:");
    ESP_LOGD(TAG, "    Tank A: %.1f%%, %.1fkg, %.1fL", 
             state->tanks_levels.tank_a.level_percentage,
             state->tanks_levels.tank_a.weight_kg,
             state->tanks_levels.tank_a.volume_liters);
    // ... (additional tank logging)

    ESP_LOGD(TAG, "  System health:");
    ESP_LOGD(TAG, "    Healthy: %s", state->system_health.system_healthy ? "Yes" : "No");
    ESP_LOGD(TAG, "    Uptime: %lu seconds", state->system_health.uptime_seconds);
    ESP_LOGD(TAG, "    Free heap: %lu bytes", state->system_health.free_heap_size);
    ESP_LOGD(TAG, "    Min free heap: %lu bytes", state->system_health.min_free_heap_size);

    ESP_LOGD(TAG, "  Error stats:");
    ESP_LOGD(TAG, "    Total errors: %lu", state->error_state.error_stats.total_errors);
    if(state->error_state.error_stats.total_errors > 0) {
        ESP_LOGD(TAG, "    Last error code: 0x%X", state->error_state.error_stats.last_error_code);
        ESP_LOGD(TAG, "    Last error time: %lu", state->error_state.error_stats.last_error_timestamp);
    }

    // Thread-safe state update
    if (state_mutex != NULL && xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&last_received_state, state, sizeof(slave_pcb_state_t));
        xSemaphoreGive(state_mutex);
    }
}

// Handle received command message
static void handle_command(const comm_command_msg_t* cmd_msg, const char* source_ip, uint16_t source_port) {
    ESP_LOGI(TAG, "Received command %d with sequence %d from %s:%d", 
             cmd_msg->command, cmd_msg->header.sequence, source_ip, source_port);
    
    // Prepare response
    comm_response_msg_t response = {
        .header = {
            .type = MSG_TYPE_ACK,
            .sequence = cmd_msg->header.sequence,
            .length = sizeof(comm_response_msg_t) - sizeof(comm_msg_header_t),
            .timestamp = esp_timer_get_time() / 1000
        },
        .command_sequence = cmd_msg->header.sequence,
        .status = SLAVE_PCB_OK
    };

    // Process command
    esp_err_t ret = process_command(cmd_msg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to process command %d: %s", 
                 cmd_msg->command, esp_err_to_name(ret));
        response.status = (slave_pcb_err_t)ret;
    }

    // Send response
    esp_err_t err = ethernet_send((const uint8_t*)&response, sizeof(response), 
                                source_ip, source_port);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send command response: %s", esp_err_to_name(err));
    }
}

// Ethernet receive callback
static void on_ethernet_data(const uint8_t *data, uint32_t length, 
                           const char *source_ip, uint16_t source_port)
{
    ESP_LOGD(TAG, "Received %d bytes from %s:%d", length, source_ip, source_port);

    // Check if it's a protocol message
    if (length >= sizeof(comm_msg_header_t)) {
        const comm_msg_header_t* header = (const comm_msg_header_t*)data;
        
        // Log message details
        ESP_LOGD(TAG, "Message Header: type=0x%02x, seq=%u, len=%u, time=%lu", 
                 header->type, header->sequence, header->length, header->timestamp);

        // Verify message length
        size_t expected_length = sizeof(comm_msg_header_t) + header->length;
        if (length != expected_length) {
            ESP_LOGW(TAG, "Message length mismatch: got %u, expected %u", 
                     (unsigned)length, (unsigned)expected_length);
            return;
        }

        switch (header->type) {
            case MSG_TYPE_STATE: {
                const comm_state_msg_t* state_msg = (const comm_state_msg_t*)data;
                handle_state_message(&state_msg->state);
                // Print error state
                print_slave_error_state(&state_msg->state.error_state);
                break;
            }

            case MSG_TYPE_COMMAND: {
                if (length >= sizeof(comm_command_msg_t)) {
                    handle_command((const comm_command_msg_t*)data, source_ip, source_port);
                }
                break;
            }

            default:
                ESP_LOGW(TAG, "Unknown message type: 0x%02x", header->type);
                break;
        }
    }
    // Legacy state message handling
    else if (length == sizeof(slave_pcb_state_t)) {
        const slave_pcb_state_t* state = (const slave_pcb_state_t*)data;
        handle_state_message(state);
    }
    else {
        ESP_LOGW(TAG, "Invalid message length: %u bytes", (unsigned)length);
        ESP_LOGD(TAG, "First 16 bytes:");
        for(int i = 0; i < length && i < 16; i++) {
            printf("%02X ", data[i]);
        }
        printf("\n");
    }
}

// Initialize communication manager
esp_err_t slave_main_communication_manager_init(void)
{
    // Create mutex for state protection
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return ESP_ERR_NO_MEM;
    }

    // Configuration for MainPCB (Server)
    ethernet_config_t config = {
        .is_server = true,
        .ip_address = "192.168.1.100",
        .netmask = "255.255.255.0",
        .gateway = "192.168.1.1",
        .port = 8888,
        .mac_address = {0x02, 0x00, 0x00, 0x01, 0x01, 0x01},
        .receive_callback = on_ethernet_data
    };

    // Initialize Ethernet
    esp_err_t ret = ethernet_manager_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Ethernet initialized successfully");

    // Get and display network information
    char ip_address[16];
    uint8_t mac[6];
    
    if (ethernet_get_ip_address(ip_address, sizeof(ip_address)) == ESP_OK) {
        ESP_LOGI(TAG, "Device IP: %s", ip_address);
    }
    
    if (ethernet_get_mac_address(mac) == ESP_OK) {
        ESP_LOGI(TAG, "Device MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    return ESP_OK;
}

// Send command to slave
esp_err_t slave_main_communication_manager_send_command(comm_cmd_t command, const uint8_t* params, 
                                           uint16_t params_len, const char* slave_ip)
{
    if (params_len > 32) {
        return ESP_ERR_INVALID_ARG;
    }

    comm_command_msg_t cmd_msg = {
        .header = {
            .type = MSG_TYPE_COMMAND,
            .sequence = ++sequence_counter,
            .length = sizeof(comm_command_msg_t) - sizeof(comm_msg_header_t) + params_len,
            .timestamp = esp_timer_get_time() / 1000
        },
        .command = command
    };

    if (params && params_len > 0) {
        memcpy(cmd_msg.parameters, params, params_len);
    }

    ESP_LOGI(TAG, "Sending command %d to %s with sequence %u", 
             command, slave_ip, cmd_msg.header.sequence);
             
    return ethernet_send((const uint8_t*)&cmd_msg, sizeof(cmd_msg), slave_ip, 8888);
}

// Get last received state
esp_err_t slave_main_communication_manager_get_last_state(slave_pcb_state_t* state)
{
    if (!state || !state_mutex) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(state, &last_received_state, sizeof(slave_pcb_state_t));
        xSemaphoreGive(state_mutex);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }

    return ret;
}
