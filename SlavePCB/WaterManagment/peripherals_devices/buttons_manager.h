#ifndef BUTTONS_MANAGER_H
#define BUTTONS_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"


#include "button_executor.h"
#include "../common_includes/buttons.h"
#include "../common_includes/cases.h"
#include "../common_includes/error_manager.h"

slave_pcb_err_t buttons_manager_init(void);
void buttons_manager_task(void *pvParameters);
void register_click_callback(void (*callback)(button_type_t, click_type_t));


#endif // BUTTONS_MANAGER_H