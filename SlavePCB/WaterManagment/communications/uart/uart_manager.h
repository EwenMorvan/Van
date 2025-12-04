#ifndef UART_MANAGER_H
#define UART_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"

#include "../../common_includes/devices.h"
#include "../../common_includes/buttons.h"
#include "../../common_includes/error_manager.h"




#define UART_NUM UART_NUM_0
#define UART_BAUD_RATE 115200
#define UART_RX_BUFFER_SIZE 1024
#define UART_QUEUE_SIZE 20

typedef enum {
    UART_CMD_BUTTON_E1 = 0,
    UART_CMD_BUTTON_E2,
    UART_CMD_BUTTON_D1,
    UART_CMD_BUTTON_D2,
    UART_CMD_BUTTON_BH,
    UART_CMD_BUTTON_V1,
    UART_CMD_BUTTON_V2,
    UART_CMD_BUTTON_P1,
    UART_CMD_BUTTON_RST,
    UART_CMD_UNKNOWN
} uart_button_cmd_t;


slave_pcb_err_t uart_manager_init(void);
bool uart_manager_get_button_state(uart_button_cmd_t button);
void uart_manager_clear_button_states(void);

#endif