#ifndef FAN_MANAGER_H
#define FAN_MANAGER_H


#include "esp_err.h"
#include "driver/gpio.h"
#include "../common_includes/gpio_pinout.h"


esp_err_t fan_manager_init(void);
esp_err_t fan_manager_set_speed(uint8_t speed_percent);
uint8_t fan_manager_get_speed(void);

#endif // FAN_MANAGER_H