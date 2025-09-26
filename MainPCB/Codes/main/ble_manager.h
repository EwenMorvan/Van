#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include "esp_err.h"
#include "protocol.h"

#define BLE_DEVICE_NAME "VanController"
#define BLE_SERVICE_UUID "12345678-1234-1234-1234-123456789abc"

esp_err_t ble_manager_init(void);
void ble_manager_task(void *parameters);
esp_err_t ble_send_state(van_state_t *state);

#endif // BLE_MANAGER_H
