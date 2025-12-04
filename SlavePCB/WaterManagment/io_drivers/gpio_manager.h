#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include "esp_log.h"
#include "driver/gpio.h"

#include "sdkconfig.h"
#include "../common_includes/gpio_pinout.h"
#include "../common_includes/error_manager.h"


slave_pcb_err_t init_gpio(void);

#endif // GPIO_MANAGER_H