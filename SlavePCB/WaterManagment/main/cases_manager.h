#ifndef CASES_MANAGER_H
#define CASES_MANAGER_H
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "../common_includes/buttons.h"
#include "../common_includes/cases.h"
#include "../common_includes/error_manager.h"
#include "../peripherals_devices/buttons_manager.h"
#include "../peripherals_devices/electrovalves_pumps_manager.h"
#include "../communications/communications_manager.h"


// System states
typedef enum {
    STATE_CE = (1 << 0),  // Clean Water Empty
    STATE_DF = (1 << 1),  // Dirt Water Full
    STATE_DE = (1 << 2),  // Dirt Water Empty
    STATE_RF = (1 << 3),  // Recycled Water Full
    STATE_RE = (1 << 4)   // Recycled Water Empty
} system_states_t;

// Case compatibility matrix
static const uint32_t incompatible_cases[CASE_MAX] = {
    [CASE_RST] = 0,
    [CASE_E1] = STATE_CE | STATE_DF,
    [CASE_E2] = STATE_CE | STATE_RF,
    [CASE_E3] = STATE_DF | STATE_RE,
    [CASE_E4] = STATE_RF | STATE_RE,
    [CASE_D1] = STATE_CE | STATE_DF,
    [CASE_D2] = STATE_CE | STATE_RF,
    [CASE_D3] = STATE_DF | STATE_RE,
    [CASE_D4] = STATE_RF | STATE_RE,
    [CASE_V1] = STATE_DE,
    [CASE_V2] = STATE_RE,
    [CASE_P1] = STATE_RF
};

slave_pcb_err_t cases_manager_init(void);
slave_pcb_err_t apply_case_logic(system_case_t case_id);

#endif // CASES_MANAGER_H