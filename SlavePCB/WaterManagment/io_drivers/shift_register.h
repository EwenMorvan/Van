#ifndef SHIFT_REGISTER_H
#define SHIFT_REGISTER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


#include "esp_rom_sys.h" 
#include "esp_log.h"
#include "driver/gpio.h"


#include "sdkconfig.h"
#include "gpio_pinout.h"
#include "devices.h"
#include "../common_includes/error_manager.h"

// Shift register chain configuration
#define SHIFT_REG_BITS_PER_REGISTER 8
#define SHIFT_REG_NUM_REGISTERS 4  // Total of 32 output bits
#define SHIFT_REG_TOTAL_BITS (SHIFT_REG_BITS_PER_REGISTER * SHIFT_REG_NUM_REGISTERS)

// Device to bit mapping
typedef struct {
    uint8_t register_index;
    uint8_t bit_position;
} device_bit_mapping_t;

// Function declarations
slave_pcb_err_t init_shift_registers(void);
bool get_device_state(device_type_t device);
void get_shift_register_state(void);
slave_pcb_err_t enable_shift_register_outputs(bool enable);
slave_pcb_err_t set_all_outputs_safe(void);
slave_pcb_err_t set_output_state(device_type_t device, bool state);

#endif // SHIFT_REGISTER_H
