#ifndef HEATER_MANAGER_H
#define HEATER_MANAGER_H

#include "esp_err.h"
#include "protocol.h"
#include "log_level.h"

#define HEATER_UPDATE_INTERVAL_MS 1000
#define HEATER_PID_KP 2.0f
#define HEATER_PID_KI 0.1f
#define HEATER_PID_KD 0.5f
#define HEATER_MIN_WATER_TEMP_FOR_FAN 40.0f
#define HEATER_UART_BUFFER_SIZE 256

esp_err_t heater_manager_init(void);
void heater_manager_task(void *parameters);
esp_err_t heater_set_target_temperature(float water_temp, float cabin_temp);
esp_err_t heater_set_state(bool enabled);
float heater_get_water_temperature(void);

#endif // HEATER_MANAGER_H
