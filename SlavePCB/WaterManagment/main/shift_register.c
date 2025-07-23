#include "slave_pcb.h"

#include "esp_rom_sys.h" 
static const char *TAG = "SHIFT_REG";

// Shift register chain configuration
#define SHIFT_REG_BITS_PER_REGISTER 8
#define SHIFT_REG_NUM_REGISTERS 4  // Total of 32 output bits
#define SHIFT_REG_TOTAL_BITS (SHIFT_REG_BITS_PER_REGISTER * SHIFT_REG_NUM_REGISTERS)

// Current output state buffer
static uint8_t shift_register_data[SHIFT_REG_NUM_REGISTERS] = {0};

// Device to bit mapping (this will be updated when shift register implementation is complete)
typedef struct {
    uint8_t register_index;
    uint8_t bit_position;
} device_bit_mapping_t;

// Mapping table for devices to shift register bits
static const device_bit_mapping_t device_mapping[DEVICE_MAX] = {
    // Electrovalves (Register 0)
    [DEVICE_ELECTROVALVE_A] = {0, 0},
    [DEVICE_ELECTROVALVE_B] = {0, 1},
    [DEVICE_ELECTROVALVE_C] = {0, 2},
    [DEVICE_ELECTROVALVE_D] = {0, 3},
    [DEVICE_ELECTROVALVE_E] = {0, 4},
    
    // Pumps (Register 0)
    [DEVICE_PUMP_PE] = {0, 5},
    [DEVICE_PUMP_PD] = {0, 6},
    [DEVICE_PUMP_PV] = {0, 7},
    [DEVICE_PUMP_PP] = {1, 0},
    
    // Button LEDs (Registers 1-3)
    [DEVICE_LED_BE1_RED]   = {1, 1},
    [DEVICE_LED_BE1_GREEN] = {1, 2},
    [DEVICE_LED_BE2_RED]   = {1, 3},
    [DEVICE_LED_BE2_GREEN] = {1, 4},
    [DEVICE_LED_BD1_RED]   = {1, 5},
    [DEVICE_LED_BD1_GREEN] = {1, 6},
    [DEVICE_LED_BD2_RED]   = {1, 7},
    [DEVICE_LED_BD2_GREEN] = {2, 0},
    [DEVICE_LED_BH]        = {2, 1},
};

/**
 * @brief Initialize shift register control pins
 */
slave_pcb_err_t init_shift_registers(void) {
    ESP_LOGI(TAG, "Initializing shift registers");

    // Set all shift register control pins to safe states
    gpio_set_level(REG_MR, 1);    // Master Reset inactive (high)
    gpio_set_level(REG_OE, 1);    // Output Enable inactive (high) - outputs disabled initially
    gpio_set_level(REG_DS, 0);    // Data Serial low
    gpio_set_level(REG_STCP, 0);  // Storage Clock low
    gpio_set_level(REG_SHCP, 0);  // Shift Clock low

    // Clear all shift register data
    memset(shift_register_data, 0, sizeof(shift_register_data));

    // Perform a master reset pulse
    gpio_set_level(REG_MR, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(REG_MR, 1);
    vTaskDelay(pdMS_TO_TICKS(1));

    ESP_LOGI(TAG, "Shift registers initialized");
    return SLAVE_PCB_OK;
}

/**
 * @brief Shift out data to all registers in the chain
 */
static slave_pcb_err_t shift_out_data(void) {
    ESP_LOGD(TAG, "Shifting out data to registers");

    // Disable outputs during data shift
    gpio_set_level(REG_OE, 1);

    // Shift data starting from the last register (furthest from MCU)
    for (int reg = SHIFT_REG_NUM_REGISTERS - 1; reg >= 0; reg--) {
        uint8_t data = shift_register_data[reg];
        
        // Shift out 8 bits, MSB first
        for (int bit = 7; bit >= 0; bit--) {
            // Set data line
            gpio_set_level(REG_DS, (data & (1 << bit)) ? 1 : 0);
            
            // Clock pulse
            gpio_set_level(REG_SHCP, 1);
            // Small delay for setup time (could be reduced or removed for faster operation)
            esp_rom_delay_us(1);
            gpio_set_level(REG_SHCP, 0);
            esp_rom_delay_us(1);
        }
    }

    // Latch data to output registers
    gpio_set_level(REG_STCP, 1);
    esp_rom_delay_us(1);
    gpio_set_level(REG_STCP, 0);

    // Enable outputs
    gpio_set_level(REG_OE, 0);

    ESP_LOGD(TAG, "Data shifted out successfully");
    return SLAVE_PCB_OK;
}

/**
 * @brief Set bit in shift register data buffer
 */
static slave_pcb_err_t set_register_bit(uint8_t register_index, uint8_t bit_position, bool state) {
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
 */
slave_pcb_err_t shift_register_set_output_state(device_type_t device, bool state) {
    if (device >= DEVICE_MAX) {
        ESP_LOGE(TAG, "Invalid device type: %d", device);
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    const device_bit_mapping_t *mapping = &device_mapping[device];
    
    ESP_LOGD(TAG, "Setting device %d (reg:%d, bit:%d) to %s", 
             device, mapping->register_index, mapping->bit_position, 
             state ? "ON" : "OFF");

    slave_pcb_err_t ret = set_register_bit(mapping->register_index, 
                                          mapping->bit_position, state);
    if (ret != SLAVE_PCB_OK) {
        return ret;
    }

    // Update all shift registers
    return shift_out_data();
}

/**
 * @brief Get current state of a device
 */
bool get_device_state(device_type_t device) {
    if (device >= DEVICE_MAX) {
        return false;
    }

    const device_bit_mapping_t *mapping = &device_mapping[device];
    return (shift_register_data[mapping->register_index] & (1 << mapping->bit_position)) != 0;
}

/**
 * @brief Set all outputs to a known safe state
 */
slave_pcb_err_t set_all_outputs_safe(void) {
    ESP_LOGI(TAG, "Setting all outputs to safe state");

    // Clear all data
    memset(shift_register_data, 0, sizeof(shift_register_data));

    // Update shift registers
    return shift_out_data();
}

/**
 * @brief Enable/disable all shift register outputs
 */
slave_pcb_err_t enable_shift_register_outputs(bool enable) {
    gpio_set_level(REG_OE, enable ? 0 : 1);
    ESP_LOGI(TAG, "Shift register outputs %s", enable ? "ENABLED" : "DISABLED");
    return SLAVE_PCB_OK;
}

/**
 * @brief Print current shift register state for debugging
 */
void print_shift_register_state(void) {
    ESP_LOGI(TAG, "Shift Register State:");
    for (int reg = 0; reg < SHIFT_REG_NUM_REGISTERS; reg++) {
        ESP_LOGI(TAG, "  Register %d: 0x%02X", reg, shift_register_data[reg]);
    }
}
