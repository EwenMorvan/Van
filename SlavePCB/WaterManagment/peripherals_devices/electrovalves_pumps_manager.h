#ifndef ELECTROVALVES_PUMPS_MANAGER_H
#define ELECTROVALVES_PUMPS_MANAGER_H

#include "esp_log.h"

#include "../communications/i2c/i2c_manager.h"
#include "../io_drivers/current_sensor.h"
#include "../../common_includes/cases.h"
#include "../../common_includes/utils.h"
#include "electrovalve_executor.h"
#include "pump_executor.h"

slave_pcb_err_t electrovalves_pumps_init(void);
slave_pcb_err_t electrovalves_pumps_case_set(system_case_t case_id);
// void electrovalves_pumps_manager_task(void *pvParameters);

#endif