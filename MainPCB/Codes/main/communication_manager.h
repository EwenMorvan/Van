#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include "protocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define COMM_QUEUE_SIZE 20

typedef enum {
    COMM_MSG_SENSOR_UPDATE,
    COMM_MSG_MPPT_UPDATE,
    COMM_MSG_HEATER_UPDATE,
    COMM_MSG_FAN_UPDATE,
    COMM_MSG_LED_UPDATE,
    COMM_MSG_COMMAND,
    COMM_MSG_ERROR
} comm_msg_type_t;

typedef struct {
    comm_msg_type_t type;
    void *data;
    size_t data_size;
} comm_message_t;

// Function prototypes
esp_err_t communication_manager_init(void);
esp_err_t comm_send_message(comm_msg_type_t type, void *data, size_t size);
esp_err_t comm_send_command(van_command_t *cmd);
QueueHandle_t comm_get_queue(void);
void communication_manager_task(void *parameters);

#endif // COMMUNICATION_MANAGER_H
