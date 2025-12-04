#ifndef PUMP_MANAGER_H
#define PUMP_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "../common_includes/gpio_pinout.h"

esp_err_t pump_manager_init(void);
esp_err_t pump_manager_set_state(bool enabled);
bool pump_manager_get_state(void);

#endif // PUMP_MANAGER_H