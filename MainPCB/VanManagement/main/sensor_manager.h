#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "esp_err.h"
#include "protocol.h"
#include "log_level.h"

#define SENSOR_UPDATE_INTERVAL_MS 1000
#define FUEL_EMPTY_THRESHOLD 5.0f  // 5% fuel level
#define DOOR_OPEN_LIGHT_THRESHOLD 500  // Light level threshold for door detection

esp_err_t sensor_manager_init(void);
void sensor_manager_task(void *parameters);
float sensor_get_fuel_level(void);
bool sensor_is_door_open(void);
void sensor_get_mppt_70_15_data(float *solar_power, float *battery_voltage, float *battery_current, int8_t *temperature, uint8_t *state);

#endif // SENSOR_MANAGER_H
