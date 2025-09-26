#ifndef W5500_ETHERNET_H
#define W5500_ETHERNET_H

#include "esp_err.h"
#include "protocol.h"

#define W5500_IP_MAIN_PCB    "192.168.1.10"
#define W5500_IP_SLAVE_PCB   "192.168.1.11"
#define W5500_PORT           8080

esp_err_t w5500_ethernet_init(void);
esp_err_t w5500_send_state(van_state_t *state);
esp_err_t w5500_receive_command(van_command_t *cmd);
bool w5500_is_slave_connected(void);

#endif // W5500_ETHERNET_H
