#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "../../common_includes/gpio_pinout.h"
#include "../../common_includes/error_manager.h"


// Callback type for received data
typedef void (*ethernet_receive_callback_t)(const uint8_t *data, uint32_t length, const char *source_ip, uint16_t source_port);

// Ethernet configuration structure
typedef struct {
    bool is_server;
    const char *ip_address;
    const char *netmask;
    const char *gateway;
    uint16_t port;
    uint8_t mac_address[6];
    ethernet_receive_callback_t receive_callback;
} ethernet_config_t;

// API Functions
esp_err_t ethernet_manager_init(const ethernet_config_t *config);
esp_err_t ethernet_send(const uint8_t *data, uint32_t length, const char *dest_ip, uint16_t dest_port);
esp_err_t ethernet_send_broadcast(const uint8_t *data, uint32_t length, uint16_t dest_port);
void ethernet_set_receive_callback(ethernet_receive_callback_t callback);
bool ethernet_is_connected(void);
esp_err_t ethernet_get_ip_address(char *buffer, uint32_t buffer_size);
esp_err_t ethernet_get_mac_address(uint8_t *mac);

// Client configuration
extern const ethernet_config_t ETHERNET_CLIENT_CONFIG;
// Server configuration
extern const ethernet_config_t ETHERNET_SERVER_CONFIG;

#endif // ETHERNET_H