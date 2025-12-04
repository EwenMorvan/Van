#include "shift_register.h"

static const char *TAG = "SHIFT_REG";

SemaphoreHandle_t output_mutex = NULL;

// Current output state buffer
static uint8_t shift_register_data[SHIFT_REG_NUM_REGISTERS] = {0};

// Mapping table for devices to shift register bits
static const device_bit_mapping_t device_mapping[DEVICE_MAX] = {
    // Electrovalves (Register 0)
    [DEVICE_ELECTROVALVE_A] = {0, 0},
    [DEVICE_ELECTROVALVE_B] = {0, 1},
    [DEVICE_ELECTROVALVE_C] = {0, 2},
    [DEVICE_ELECTROVALVE_D] = {0, 3},
    [DEVICE_ELECTROVALVE_E] = {0, 4},
    [DEVICE_ELECTROVALVE_F] = {0, 5},

    // Pumps (Register 0)
    [DEVICE_PUMP_PE] = {0, 6},
    [DEVICE_PUMP_PV] = {0, 7},
    [DEVICE_PUMP_PD] = {1, 0},
    [DEVICE_PUMP_PP] = {1, 1},
    [DEVICE_LED_BH]  = {1, 2}, 

    // Button LEDs (Registers 1-3) - Order matches device_type_t enum in slave_pcb.h
    [DEVICE_LED_BE1_RED]     = {1, 4},  // BE1 RED -> Hardware position {1, 4}
    [DEVICE_LED_BE1_GREEN]   = {1, 3},  // BE1 GREEN -> Hardware position {1, 3}
    [DEVICE_LED_BE2_RED]     = {1, 6},  // BE2 RED -> Hardware position {1, 6}
    [DEVICE_LED_BE2_GREEN]   = {1, 5},  // BE2 GREEN -> Hardware position {1, 5}
    [DEVICE_LED_BD1_RED]     = {2, 0},  // BD1 RED -> Hardware position {2, 0}
    [DEVICE_LED_BD1_GREEN]   = {1, 7},  // BD1 GREEN -> Hardware position {1, 7}
    [DEVICE_LED_BD2_RED]     = {2, 2},  // BD2 RED -> Hardware position {2, 2}
    [DEVICE_LED_BD2_GREEN]   = {2, 1},  // BD2 GREEN -> Hardware position {2, 1}
};

////////////////////////////////////////// Internal Functions //////////////////////////////////////////
/**
 * @brief Shift out data to all registers in the chain
 */
static slave_pcb_err_t _shift_out_data(void) {
    ESP_LOGD(TAG, "Shifting out data to registers");

    // NOTE: Keep outputs enabled during shift to avoid visible glitches
    // Modern shift registers can handle this well with proper timing

    // Ensure all control lines are in proper state
    gpio_set_level(REG_DS, 0);    // Data line low initially
    gpio_set_level(REG_STCP, 0);  // Storage clock low
    gpio_set_level(REG_SHCP, 0);  // Shift clock low
    esp_rom_delay_us(10); // Let signals stabilize

    // Shift data starting from the last register (furthest from MCU)
    for (int reg = SHIFT_REG_NUM_REGISTERS - 1; reg >= 0; reg--) {
        uint8_t data = shift_register_data[reg];
        
        // Shift out 8 bits, MSB first
        for (int bit = 7; bit >= 0; bit--) {
            // Set data line with proper setup time
            gpio_set_level(REG_DS, (data & (1 << bit)) ? 1 : 0);
            esp_rom_delay_us(5); // Increased setup time for data stability
            
            // Clock pulse with proper timing
            gpio_set_level(REG_SHCP, 1);
            esp_rom_delay_us(5); // Increased clock high time
            gpio_set_level(REG_SHCP, 0);
            esp_rom_delay_us(5); // Increased clock low time
        }
    }

    // Ensure data line is stable before latching
    gpio_set_level(REG_DS, 0); // Set to known state
    esp_rom_delay_us(10); // Increased stabilization time

    // Latch data to output registers with proper timing
    gpio_set_level(REG_STCP, 1);
    esp_rom_delay_us(10); // Increased latch pulse width
    gpio_set_level(REG_STCP, 0);
    esp_rom_delay_us(10); // Increased latch setup time

    ESP_LOGD(TAG, "Data shifted out successfully");
    return SLAVE_PCB_OK;
}

/**
 * @brief Set bit in shift register data buffer
 */
static slave_pcb_err_t _set_register_bit(uint8_t register_index, uint8_t bit_position, bool state) {
    if (register_index >= SHIFT_REG_NUM_REGISTERS || bit_position >= 8) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    if (state) {
        shift_register_data[register_index] |= (1 << bit_position);
    } else {
        shift_register_data[register_index] &= ~(1 << bit_position);
    }

    ESP_LOGD(TAG, "Set register %d bit %d to %d", register_index, bit_position, state);
    return SLAVE_PCB_OK;
}

/**
 * @brief Implementation of set_output_state for shift register controlled devices
 * @param device Device type to control
 * @param state State to set (true/false, on/off)
 * @return slave_pcb_err_t Error code
 */
static slave_pcb_err_t _shift_register_set_output_state(device_type_t device, bool state) {
    if (device >= DEVICE_MAX) {
        ESP_LOGE(TAG, "Invalid device type: %d", device);
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    const device_bit_mapping_t *mapping = &device_mapping[device];
    
    ESP_LOGD(TAG, "Setting device %d (reg:%d, bit:%d) to %s", 
             device, mapping->register_index, mapping->bit_position, 
             state ? "ON" : "OFF");

    // Set the bit in the register data buffer
    slave_pcb_err_t ret = _set_register_bit(mapping->register_index, 
                                          mapping->bit_position, state);
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to set register bit for device %d", device);
        return ret;
    }

    // Update all shift registers with better error handling
    ret = _shift_out_data();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to shift out data for device %d", device);
        return ret;
    }

    ESP_LOGD(TAG, "Device %d successfully set to %s", device, state ? "ON" : "OFF");
    return SLAVE_PCB_OK;
}


////////////////////////////////////////// Public Functions //////////////////////////////////////////
/**
 * @brief Initialize shift register control pins
 * @return slave_pcb_err_t Error code
 */
slave_pcb_err_t init_shift_registers(void) {
    ESP_LOGI(TAG, "Initializing shift registers");


    output_mutex = xSemaphoreCreateMutex();
    if (output_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create output mutex");
        return SLAVE_PCB_ERR_INIT_FAIL;
    }

    // Set all shift register control pins to safe states
    gpio_set_level(REG_MR, 1);    // Master Reset inactive (high)
    gpio_set_level(REG_OE, 1);    // Output Enable inactive (high) - outputs disabled initially
    gpio_set_level(REG_DS, 0);    // Data Serial low
    gpio_set_level(REG_STCP, 0);  // Storage Clock low
    gpio_set_level(REG_SHCP, 0);  // Shift Clock low

    // Wait for signals to stabilize
    vTaskDelay(pdMS_TO_TICKS(10));

    // Clear all shift register data
    memset(shift_register_data, 0, sizeof(shift_register_data));

    // Perform a master reset pulse
    gpio_set_level(REG_MR, 0);
    vTaskDelay(pdMS_TO_TICKS(5)); // Longer reset pulse
    gpio_set_level(REG_MR, 1);
    vTaskDelay(pdMS_TO_TICKS(5)); // Wait after reset

    // Shift out initial data (all zeros) to ensure clean state
    _shift_out_data();

    // Enable outputs after everything is initialized
    gpio_set_level(REG_OE, 0);
    vTaskDelay(pdMS_TO_TICKS(5)); // Allow outputs to stabilize

    ESP_LOGI(TAG, "Shift registers initialized successfully");
    return SLAVE_PCB_OK;
}

/**
 * @brief Get current state of a device
 * @param device Device type to query
 * @return true if device is ON, false if OFF
 */
bool get_device_state(device_type_t device) {
    if (device >= DEVICE_MAX) {
        return false;
    }

    const device_bit_mapping_t *mapping = &device_mapping[device];
    return (shift_register_data[mapping->register_index] & (1 << mapping->bit_position)) != 0;
}

/**
 * @brief Get current shift register state for debugging
 * @todo change to return data instead of printing
 */
void get_shift_register_state(void) {
    ESP_LOGI(TAG, "Shift Register State:");
    for (int reg = 0; reg < SHIFT_REG_NUM_REGISTERS; reg++) {
        ESP_LOGI(TAG, "  Register %d: 0x%02X", reg, shift_register_data[reg]);
    }
}

/**
 * @brief Enable/disable all shift register outputs
 * @param enable true to enable outputs, false to disable
 * @return slave_pcb_err_t Error code
 */
slave_pcb_err_t enable_shift_register_outputs(bool enable) {
    gpio_set_level(REG_OE, enable ? 0 : 1);
    ESP_LOGI(TAG, "Shift register outputs %s", enable ? "ENABLED" : "DISABLED");
    return SLAVE_PCB_OK;
}

/**
 * @brief Set all outputs to a known safe state
 * @return slave_pcb_err_t Error code
 */
slave_pcb_err_t set_all_outputs_safe(void) {
    ESP_LOGI(TAG, "Setting all outputs to safe state");

    // Clear all data
    memset(shift_register_data, 0, sizeof(shift_register_data));

    // Update shift registers
    return _shift_out_data();
}


/**
 * @brief Generic function to set output state for any device
 * @param device Device type to control
 * @param state State to set (true/false, on/off)
 * @return slave_pcb_err_t Error code
 */
slave_pcb_err_t set_output_state(device_type_t device, bool state) {
    if (device >= DEVICE_MAX) {
        ESP_LOGE(TAG, "Invalid device type: %d", device);
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Take mutex to ensure atomic operations
    if (xSemaphoreTake(output_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take output mutex");
        return SLAVE_PCB_ERR_TIMEOUT;
    }

    slave_pcb_err_t ret = SLAVE_PCB_OK;

    ESP_LOGD(TAG, "Setting device %d to state %d", device, state);

    // Use shift register implementation for output control
    ret = _shift_register_set_output_state(device, state);
    
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to set device output state for device %d", device);
    }

    xSemaphoreGive(output_mutex);
    return ret;
}

