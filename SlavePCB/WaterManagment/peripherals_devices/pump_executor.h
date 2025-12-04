#ifndef PUMP_EXECUTOR_H
#define PUMP_EXECUTOR_H

#include "esp_log.h"
#include "esp_timer.h"

#include "../io_drivers/current_sensor.h"
#include "../io_drivers/shift_register.h"
#include "../../common_includes/error_manager.h"
#include "../../common_includes/devices.h"

typedef struct {
    bool is_active;
    bool target_state;
    uint32_t last_state_change;
    float last_current_reading;
    bool is_pumping;
} pump_state_t;

typedef struct {
    device_type_t device;
    const char* name;
    uint8_t i2c_channel;
    bool has_current_sensor;
    uint8_t pumping_current_threshold_ma;
} pump_config_t;


slave_pcb_err_t pump_init(void);
slave_pcb_err_t pump_set_state(device_type_t device, bool state);
bool pump_is_pumping(device_type_t device);
float pump_get_current(device_type_t device);
const pump_config_t* pump_get_config(device_type_t device);
bool pump_is_valid_device(device_type_t device);

#endif