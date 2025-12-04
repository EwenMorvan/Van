#ifndef ELECTROVALVE_EXECUTOR_H
#define ELECTROVALVE_EXECUTOR_H

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
    bool is_turning;
} electrovalve_state_t;

typedef struct {
    device_type_t device;
    const char* name;
    uint8_t i2c_channel;
    bool has_current_sensor;
} electrovalve_config_t;

slave_pcb_err_t electrovalve_init(void);
slave_pcb_err_t electrovalve_set_state(device_type_t device, bool state);
bool electrovalve_is_turning(device_type_t device);
float electrovalve_get_current(device_type_t device);
const electrovalve_config_t* electrovalve_get_config(device_type_t device);
bool electrovalve_is_valid_device(device_type_t device);

#endif // ELECTROVALVE_EXECUTOR_H