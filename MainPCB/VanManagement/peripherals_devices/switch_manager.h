#ifndef SWITCH_MANAGER_H
#define SWITCH_MANAGER_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "../common_includes/gpio_pinout.h"
#include "../Communications/Uart/uart_multiplexer.h"
#include "../main/global_coordinator.h"
#define SWITCH_DEBOUNCE_MS 50        
#define SWITCH_SHORT_PRESS_MS 500
#define SWITCH_MULTI_CLICK_MS 700    
#define SWITCH_LONG_PRESS_MS 1000    
#define SWITCH_LONG_CYCLE_MS 5000    // Durée d'un aller complet 0→100→0

esp_err_t switch_manager_init(void);
void switch_manager_task(void *parameters);
bool get_door_state(void);


#endif // SWITCH_MANAGER_H