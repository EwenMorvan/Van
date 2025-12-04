#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdbool.h>

#include "ethernet/ethernet_manager.h"
#include "../common_includes/error_manager.h"
#include "../common_includes/slave_pcb_res/slave_pcb_error_manager.h"
#include "../common_includes/slave_pcb_res/slave_pcb_state.h"
#include "../common_includes/slave_pcb_res/slave_pcb_cases.h"


#include "../peripherals_devices/hood_manager.h"



// Protocol message types
#define MSG_TYPE_COMMAND 0x01
#define MSG_TYPE_ACK 0x02
#define MSG_TYPE_NACK 0x03
#define MSG_TYPE_STATE 0x04


// Commands
typedef enum {
    CMD_SET_HOOD_OFF = 0,
    CMD_SET_HOOD_ON,
    CMD_MAX
} comm_cmd_t;


// Protocol message structures
typedef struct __attribute__((packed)) {
    uint8_t type;           // Message type
    uint16_t sequence;      // Sequence number
    uint16_t length;        // Length of payload
    uint32_t timestamp;     // Message timestamp
} comm_msg_header_t;

// Initialize communication manager
esp_err_t slave_main_communication_manager_init(void);

// Send command to slave PCB
esp_err_t slave_main_communication_manager_send_command(comm_cmd_t command, const uint8_t* params, 
                                           uint16_t params_len, const char* slave_ip);

// Get last received state (thread-safe)
esp_err_t slave_main_communication_manager_get_last_state(slave_pcb_state_t* state);

#endif // COMMUNICATION_MANAGER_H