#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include <stdint.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "../communications/i2c/i2c_manager.h"
#include "../common_includes/error_manager.h"

#define CURRENT_SENSOR_ADDR  0x40





// INA219 registers
#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNTVOLTAGE 0x01
#define INA219_REG_BUSVOLTAGE   0x02
#define INA219_REG_POWER        0x03
#define INA219_REG_CURRENT      0x04
#define INA219_REG_CALIBRATION  0x05

// Shunt resistor values
#define SHUNT_RESISTOR_ELECTROVALVE 0.1f
#define SHUNT_RESISTOR_PUMP_PE_PV 0.08f
#define SHUNT_RESISTOR_PUMP_OTHER 0.0f

// Max current values
#define MAX_CURRENT_ELECTROVALVE_AMP 0.5f
#define MAX_CURRENT_PUMP_PE_AMP      4.0f
#define MAX_CURRENT_PUMP_PV_AMP      2.0f

#define CURRENT_THRESHOLD_EV_MA 6
#define CURRENT_THRESHOLD_PUMP_PE_MA 100
#define CURRENT_THRESHOLD_PUMP_PV_EMPTY_MA 100
#define CURRENT_THRESHOLD_PUMP_PV_WATER_MA 500

slave_pcb_err_t current_sensor_init(void);
slave_pcb_err_t current_sensor_read_channel(uint8_t channel, float *current_ma);
float get_shunt_resistor_for_channel(uint8_t channel);

#endif