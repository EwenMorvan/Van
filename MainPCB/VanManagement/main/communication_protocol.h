#ifndef COMMUNICATION_PROTOCOL_H
#define COMMUNICATION_PROTOCOL_H

#include "protocol.h"
#include "esp_err.h"
#include <stdint.h>
#include "log_level.h"

// Maximum JSON message size
#define MAX_JSON_MESSAGE_SIZE 2048
#define MAX_COMMAND_BUFFER_SIZE 512

// External communication message types
typedef enum {
    MSG_TYPE_STATE = 0,      // Periodic state update
    MSG_TYPE_COMMAND,        // Command from app
    MSG_TYPE_RESPONSE,       // Response to command
    MSG_TYPE_ERROR          // Error message
} external_msg_type_t;

// Communication interface type
typedef enum {
    COMM_INTERFACE_BLE = 0,
    COMM_INTERFACE_USB,
    COMM_INTERFACE_MAX
} comm_interface_t;

// Function pointer for sending data
typedef esp_err_t (*send_data_func_t)(const char* data, size_t length);

// Communication interface structure
typedef struct {
    comm_interface_t type;
    send_data_func_t send_func;
    bool is_connected;
    uint32_t last_state_sent;
    uint32_t state_interval_ms;
} comm_interface_config_t;

// Protocol functions
esp_err_t comm_protocol_init(void);
esp_err_t comm_protocol_start(void);
esp_err_t comm_protocol_register_interface(comm_interface_t interface, send_data_func_t send_func);
esp_err_t comm_protocol_set_connected(comm_interface_t interface, bool connected);
esp_err_t comm_protocol_process_received_data(comm_interface_t interface, const char* data, size_t length);

// JSON serialization functions  
esp_err_t comm_serialize_state(van_state_t *state, char *json_buffer, size_t buffer_size);
esp_err_t comm_serialize_response(const char* status, const char* message, char *json_buffer, size_t buffer_size);
esp_err_t comm_serialize_error(uint32_t error_code, const char* error_message, char *json_buffer, size_t buffer_size);

// JSON parsing functions
esp_err_t comm_parse_command(const char* json_data, van_command_t *command);

// Periodic state broadcasting
void comm_protocol_task(void *parameters);
esp_err_t comm_broadcast_state(void);
esp_err_t comm_send_response(comm_interface_t interface, const char* status, const char* message);

#endif // COMMUNICATION_PROTOCOL_H
