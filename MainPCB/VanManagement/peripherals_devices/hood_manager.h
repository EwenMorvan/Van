#ifndef HOOD_MANAGER_H
#define HOOD_MANAGER_H

#include "esp_err.h"
#include <stdint.h>

#include "../common_includes/slave_pcb_res/slave_pcb_state.h" // For hood_state_t
#include "../common_includes/gpio_pinout.h"


// Functions to control the hood
esp_err_t hood_init(void);
void hood_set_state(hood_state_t state);

#endif // HOOD_MANAGER_H