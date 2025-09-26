#ifndef BLE_COMM_H
#define BLE_COMM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration BLE
#define BLE_DEVICE_NAME_SERVER "ESP32_Server"
#define BLE_DEVICE_NAME_CLIENT "ESP32_Client"
#define BLE_SERVICE_UUID        0x1234
#define BLE_CHAR_UUID           0x5678
#define BLE_MAX_DATA_LEN        512

// États de connexion
typedef enum {
    BLE_STATE_IDLE = 0,
    BLE_STATE_ADVERTISING,
    BLE_STATE_SCANNING,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_DISCONNECTED
} ble_state_t;

// Structure de communication BLE
typedef struct {
    bool is_server;                    // true = serveur, false = client
    ble_state_t state;                 // État actuel
    uint16_t conn_handle;              // Handle de connexion
    SemaphoreHandle_t mutex;           // Mutex pour thread safety
    SemaphoreHandle_t data_ready_sem;  // Sémaphore pour données reçues
    char rx_buffer[BLE_MAX_DATA_LEN];  // Buffer de réception
    size_t rx_len;                     // Taille des données reçues
    bool connected;                    // Flag de connexion
} ble_comm_t;

// Callbacks
typedef void (*ble_data_received_cb_t)(const char* data, size_t len);
typedef void (*ble_connection_cb_t)(void);

// Variables globales pour les callbacks
extern ble_data_received_cb_t g_data_received_cb;
extern ble_connection_cb_t g_connected_cb;
extern ble_connection_cb_t g_disconnected_cb;

// Fonctions principales
esp_err_t ble_comm_init(ble_comm_t *comm, bool is_server);
void ble_comm_set_callbacks(ble_connection_cb_t connected_cb, ble_connection_cb_t disconnected_cb, ble_data_received_cb_t data_cb);
esp_err_t ble_comm_start(ble_comm_t *comm);
esp_err_t ble_comm_stop(ble_comm_t *comm);
esp_err_t ble_comm_send(ble_comm_t *comm, const char *data, size_t len);
esp_err_t ble_comm_recv(ble_comm_t *comm, char *buffer, size_t buffer_len, size_t *recv_len);
void ble_comm_deinit(ble_comm_t *comm);

// Configuration des callbacks
void ble_comm_set_data_callback(ble_data_received_cb_t callback);
void ble_comm_set_connection_callback(ble_connection_cb_t callback);

// Utilitaires
bool ble_comm_is_connected(ble_comm_t *comm);
const char* ble_comm_get_state_string(ble_state_t state);

#ifdef __cplusplus
}
#endif

#endif // BLE_COMM_H