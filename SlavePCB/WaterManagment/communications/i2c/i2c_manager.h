#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H
#include <stdint.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"

#include "../../common_includes/error_manager.h"
#include "../../common_includes/gpio_pinout.h"


slave_pcb_err_t i2c_manager_init(void);
slave_pcb_err_t i2c_set_multiplexer_channel(uint8_t channel);
slave_pcb_err_t i2c_write_register(uint8_t device_addr, uint8_t reg, uint16_t value);
slave_pcb_err_t i2c_read_register(uint8_t device_addr, uint8_t reg, uint16_t *value);

#endif // I2C_MANAGER_H