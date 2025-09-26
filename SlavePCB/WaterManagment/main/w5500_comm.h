#ifndef W5500_COMM_H
#define W5500_COMM_H

#include <esp_eth.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <driver/gpio.h>  // Pour gpio_num_t
#include <driver/spi_master.h>  // Pour spi_host_device_t
#include <errno.h>  // Pour errno

typedef struct {
    esp_eth_handle_t eth_handle;
    esp_netif_t *netif;
    int socket;
    SemaphoreHandle_t mutex;  // Pour thread-safety
    uint8_t is_server;  // 1 si serveur (ESP A), 0 si client (ESP B)
    const char *remote_ip;  // IP distante
    uint16_t remote_port;   // Port distant
    uint16_t local_port;    // Port local
} w5500_comm_t;

esp_err_t w5500_comm_init(w5500_comm_t *comm, uint8_t is_server, const char *remote_ip, uint16_t port);
void w5500_comm_deinit(w5500_comm_t *comm);
esp_err_t w5500_comm_send(w5500_comm_t *comm, const char *msg, size_t len);
esp_err_t w5500_comm_recv(w5500_comm_t *comm, char *buf, size_t buf_len, size_t *recv_len);

#endif