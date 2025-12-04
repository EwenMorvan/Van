#ifndef COMMUNICATIONS_MANAGER_H
#define COMMUNICATIONS_MANAGER_H

#include <stdint.h> 
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "../common_includes/error_manager.h"
#include "../common_includes/cases.h"
#include "ethernet/ethernet_manager.h"

#define MAIN_PCB_IP       "192.168.1.100"
#define MAIN_PCB_PORT     8888
#define COMM_STATE_UPDATE_PERIOD_MS 1000
#define COMM_COMMAND_TIMEOUT_MS 5000
#define COMM_MAX_RETRIES 3

// Protocol message types
#define MSG_TYPE_COMMAND 0x01
#define MSG_TYPE_ACK 0x02
#define MSG_TYPE_NACK 0x03
#define MSG_TYPE_STATE 0x04



typedef enum {
    CMD_SET_HOOD_OFF = 0,
    CMD_SET_HOOD_ON,
    CMD_MAX
    
} comm_cmd_t;

typedef enum {
    HOOD_OFF = 0,
    HOOD_ON
    
} hood_state_t;




// System health status
typedef struct {
    bool system_healthy;
    uint32_t last_health_check;
    uint32_t uptime_seconds;
    uint32_t free_heap_size;
    uint32_t min_free_heap_size;
} slave_health_t;

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

// API d'accès à l'état global
slave_pcb_state_t* get_system_state(void);
void update_system_case(system_case_t new_case);
void update_hood_state(hood_state_t new_state);
void update_tank_levels(const water_tanks_levels_t* new_levels);
void update_system_health(const slave_health_t* new_health);
void update_error_state(const slave_error_state_t* new_error_state);

// Protocol message header
typedef struct __attribute__((packed)) {
    uint8_t type;           // Message type (command, ack, state, etc.)
    uint16_t sequence;      // Sequence number for tracking
    uint16_t length;        // Length of payload
    uint32_t timestamp;     // Message timestamp
} comm_msg_header_t;

// Command message
typedef struct __attribute__((packed)) {
    comm_msg_header_t header;
    comm_cmd_t command;
    uint8_t parameters[32];  // Flexible parameter buffer
} comm_command_msg_t;

// Response message
typedef struct __attribute__((packed)) {
    comm_msg_header_t header;
    uint16_t command_sequence; // Original command sequence number
    slave_pcb_err_t status;   // Command execution status
} comm_response_msg_t;

// State message
typedef struct __attribute__((packed)) {
    comm_msg_header_t header;
    slave_pcb_state_t state;
} comm_state_msg_t;

// Initialize the communications manager
esp_err_t communications_manager_init(void);

// Send a command and wait for acknowledgment
esp_err_t communications_send_command_with_ack(comm_cmd_t command, const uint8_t *params, 
                                             uint16_t params_len, uint32_t timeout_ms);

// Send the current system state
esp_err_t communications_send_state(void);

// Start the state update task
esp_err_t communications_start_state_update_task(void);

// Get the last communication error
slave_pcb_err_t communications_get_last_error(void);

// Register callback for receiving commands
typedef void (*comm_command_callback_t)(comm_cmd_t command, const uint8_t *params, uint16_t params_len);
esp_err_t communications_register_command_callback(comm_command_callback_t callback);

#endif // COMMUNICATIONS_MANAGER_H