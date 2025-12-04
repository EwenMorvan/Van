#ifndef HEATER_MANAGER_H
#define HEATER_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include <math.h>

#include "../common_includes/gpio_pinout.h"
#include "../communications/protocol.h"
#include "fan_manager.h"
#include "pump_manager.h"

esp_err_t heater_manager_init(void);
uint8_t heater_manager_get_fuel_level(void);
esp_err_t heater_manager_set_air_heater(bool state, uint8_t fan_speed_percent);
esp_err_t heater_manager_set_diesel_water_heater(bool state, uint8_t temperature);
esp_err_t heater_manager_update_van_state(van_state_t* van_state);

#endif // HEATER_MANAGER_H