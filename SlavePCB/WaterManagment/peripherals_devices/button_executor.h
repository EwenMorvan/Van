#ifndef BUTTON_EXECUTOR_H
#define BUTTON_EXECUTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "sdkconfig.h"
#include "../io_drivers/shift_register.h"
#include "../common_includes/buttons.h"
#include "../common_includes/error_manager.h"
#include "../common_includes/devices.h"
#include "../common_includes/utils.h"
#include "../communications/uart/uart_manager.h"


// Buttons LED color definitions for each case
typedef struct {
    bool be1_red;
    bool be1_green;
    bool be2_red;
    bool be2_green;
    bool bd1_red;
    bool bd1_green;
    bool bd2_red;
    bool bd2_green;
} led_colors_t;

static const led_colors_t case_led_colors[CASE_MAX] = {
    [CASE_RST] = {0, 0, 0, 0, 0, 0, 0, 0}, // All off
    [CASE_E1]  = {0, 1, 1, 0, 0, 0, 0, 0}, // BE1 Green, BE2 Red
    [CASE_E2]  = {0, 1, 0, 1, 0, 0, 0, 0}, // BE1 Green, BE2 Green
    [CASE_E3]  = {1, 0, 1, 0, 0, 0, 0, 0}, // BE1 Red, BE2 Red
    [CASE_E4]  = {1, 0, 0, 1, 0, 0, 0, 0}, // BE1 Red, BE2 Green
    [CASE_D1]  = {0, 0, 0, 0, 0, 1, 1, 0}, // BD1 Green, BD2 Red
    [CASE_D2]  = {0, 0, 0, 0, 0, 1, 0, 1}, // BD1 Green, BD2 Green
    [CASE_D3]  = {0, 0, 0, 0, 1, 0, 1, 0}, // BD1 Red, BD2 Red
    [CASE_D4]  = {0, 0, 0, 0, 1, 0, 0, 1}, // BD1 Red, BD2 Green
    [CASE_V1]  = {0, 0, 0, 0, 0, 0, 0, 0}, // All off
    [CASE_V2]  = {0, 0, 0, 0, 0, 0, 0, 0}, // All off
    [CASE_P1]  = {0, 0, 0, 0, 0, 0, 0, 0}, // All off
};

// Function declarations
click_type_t detect_button_click(button_type_t button, button_state_t *state);
slave_pcb_err_t button_set_leds(system_case_t case_id);
slave_pcb_err_t set_leds_transitioning(void);
slave_pcb_err_t button_bh_set_led(bool state);
#endif // BUTTON_EXECUTOR_H