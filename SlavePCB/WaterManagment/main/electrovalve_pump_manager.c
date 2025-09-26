#include "slave_pcb.h"

static const char *TAG = "EV_PUMP_MGR";

// I2C configuration
#define I2C_MASTER_SCL_IO    I2C_MUX_SCL
#define I2C_MASTER_SDA_IO    I2C_MUX_SDA
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

// I2C Multiplexer address
#define I2C_MULTIPLEXER_ADDR 0x70
// Current sensor I2C address (behind multiplexer)
#define CURRENT_SENSOR_ADDR  0x40

// Current threshold for detecting if electrovalve/pump is active
#define CURRENT_THRESHOLD_MA 100

// INA219 calibration values for different shunt resistors
#define INA219_CURRENT_DIVIDER_MA 10.0f     // Fixed divider for INA219
#define INA219_POWER_MULTIPLIER_MW 2.0f     // Fixed multiplier for INA219

// Shunt resistor values (in ohms)
#define SHUNT_RESISTOR_ELECTROVALVE 0.1f    // 100mΩ for electrovalves
#define SHUNT_RESISTOR_PUMP_PE_PV 0.08f     // 80mΩ for pumps PE and PV
#define SHUNT_RESISTOR_PUMP_OTHER 0.0f      // No sensors for PD and PP

// INA219 registers
#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNTVOLTAGE 0x01
#define INA219_REG_BUSVOLTAGE   0x02
#define INA219_REG_POWER        0x03
#define INA219_REG_CURRENT      0x04
#define INA219_REG_CALIBRATION  0x05

// INA219 configuration
#define INA219_CONFIG_RESET     0x8000
#define INA219_CONFIG_BVOLTAGERANGE_32V 0x2000
#define INA219_CONFIG_GAIN_8_320MV 0x1800
#define INA219_CONFIG_BADCRES_12BIT 0x0400
#define INA219_CONFIG_SADCRES_12BIT_1S_532US 0x0018
#define INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS 0x0007

// Device state tracking
typedef struct {
    bool is_active;
    bool target_state;
    uint32_t last_state_change;
    float last_current_reading;
    bool is_turning; // For electrovalves
    bool is_pumping; // For pumps
} device_state_t;

static device_state_t electrovalve_states[5]; // A, B, C, D, E
static device_state_t pump_states[4]; // PE, PD, PV, PP

// Forward declarations
static slave_pcb_err_t read_current_sensor_alternative(uint8_t channel, float *current_ma);

/**
 * @brief Write register to INA219
 */
static slave_pcb_err_t ina219_write_register(uint8_t reg, uint16_t value) {
    uint8_t data[3];
    data[0] = reg;
    data[1] = (value >> 8) & 0xFF;  // MSB first
    data[2] = value & 0xFF;         // LSB second
    
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, CURRENT_SENSOR_ADDR, 
                                              data, 3, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "INA219 write register 0x%02X failed: %s", reg, esp_err_to_name(ret));
        return SLAVE_PCB_ERR_I2C_FAIL;
    }
    
    return SLAVE_PCB_OK;
}

/**
 * @brief Read register from INA219
 */
static slave_pcb_err_t ina219_read_register(uint8_t reg, uint16_t *value) {
    if (!value) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }
    
    // Write register address
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, CURRENT_SENSOR_ADDR, 
                                              &reg, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "INA219 write register address failed: %s", esp_err_to_name(ret));
        return SLAVE_PCB_ERR_I2C_FAIL;
    }
    
    // Read register value
    uint8_t data[2];
    ret = i2c_master_read_from_device(I2C_MASTER_NUM, CURRENT_SENSOR_ADDR, 
                                     data, 2, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "INA219 read register data failed: %s", esp_err_to_name(ret));
        return SLAVE_PCB_ERR_I2C_FAIL;
    }
    
    *value = (data[0] << 8) | data[1];  // MSB first
    return SLAVE_PCB_OK;
}

/**
 * @brief Initialize and calibrate INA219 for specific shunt resistor
 */
static slave_pcb_err_t ina219_calibrate(float shunt_resistor_ohms) {
    if (shunt_resistor_ohms <= 0) {
        ESP_LOGW(TAG, "Invalid shunt resistor value: %.3f ohms", shunt_resistor_ohms);
        return SLAVE_PCB_ERR_INVALID_ARG;
    }
    
    // Reset INA219
    slave_pcb_err_t ret = ina219_write_register(INA219_REG_CONFIG, INA219_CONFIG_RESET);
    if (ret != SLAVE_PCB_OK) {
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for reset
    
    // Configure INA219 with appropriate gain based on expected current
    // For water pumps, we expect 1-3A typically, so use ±160mV range (gain /2)
    // For electrovalves, we expect 0.5-1.5A, so use ±80mV range (gain /4) 
    uint16_t config;
    if (shunt_resistor_ohms >= 0.09f) {
        // Electrovalves: 100mΩ shunt, expect ~1A, use ±80mV range
        config = INA219_CONFIG_BVOLTAGERANGE_32V |
                0x1000 |  // GAIN_4_160MV (±160mV range)
                INA219_CONFIG_BADCRES_12BIT |
                INA219_CONFIG_SADCRES_12BIT_1S_532US |
                INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
    } else {
        // Pumps: 80mΩ shunt, expect ~2A, use ±160mV range  
        config = INA219_CONFIG_BVOLTAGERANGE_32V |
                0x1000 |  // GAIN_4_160MV (±160mV range)
                INA219_CONFIG_BADCRES_12BIT |
                INA219_CONFIG_SADCRES_12BIT_1S_532US |
                INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
    }
                     
    ret = ina219_write_register(INA219_REG_CONFIG, config);
    if (ret != SLAVE_PCB_OK) {
        return ret;
    }
    
    // Calculate calibration value using realistic current ranges
    // For electrovalves: max 2A (more reasonable for 24V solenoids)
    // For pumps: max 4A (more realistic for small water pumps)
    float max_current_a = (shunt_resistor_ohms >= 0.09f) ? 2.0f : 4.0f;
    
    // Calculate Current LSB: we want good resolution
    // Use smaller divisor for better resolution
    float current_lsb = max_current_a / 4096.0f;  // Use 12-bit resolution instead of 15-bit
    
    // Calculate calibration register value
    // Cal = 0.04096 / (Current_LSB * Rshunt)
    uint16_t cal = (uint16_t)(0.04096f / (current_lsb * shunt_resistor_ohms));
    
    // Ensure calibration value is within valid range
    if (cal > 0xFFFE) {
        cal = 0xFFFE;
    } else if (cal < 1) {
        cal = 1;
    }
    
    ret = ina219_write_register(INA219_REG_CALIBRATION, cal);
    if (ret != SLAVE_PCB_OK) {
        return ret;
    }
    
    // ESP_LOGI(TAG, "INA219 calibrated: Rshunt=%.3fΩ, MaxI=%.1fA, LSB=%.1fµA, Cal=0x%04X", 
    //          shunt_resistor_ohms, max_current_a, current_lsb * 1000000, cal);
    
    return SLAVE_PCB_OK;
}

/**
 * @brief Get shunt resistor value for specific channel
 */
static float get_shunt_resistor_for_channel(uint8_t channel) {
    if (channel < 5) {
        // Channels 0-4: Electrovalves A-E (100mΩ shunt resistors)
        return SHUNT_RESISTOR_ELECTROVALVE;
    } else if (channel == 5) {
        // Channel 5: Pump PE (80mΩ shunt resistor)
        return SHUNT_RESISTOR_PUMP_PE_PV;
    } else if (channel == 7) {
        // Channel 7: Pump PV (80mΩ shunt resistor)
        return SHUNT_RESISTOR_PUMP_PE_PV;
    } else {
        // Channels 6, 8: Pumps PD, PP (no current sensors)
        return SHUNT_RESISTOR_PUMP_OTHER;
    }
}

/**
 * @brief Initialize I2C for current sensors
 */
static slave_pcb_err_t init_i2c(void) {
    ESP_LOGI(TAG, "Initializing I2C on SDA=%d, SCL=%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return SLAVE_PCB_ERR_I2C_FAIL;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 
                           I2C_MASTER_RX_BUF_DISABLE, 
                           I2C_MASTER_TX_BUF_DISABLE, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return SLAVE_PCB_ERR_I2C_FAIL;
    }

    ESP_LOGI(TAG, "I2C initialized successfully");
    
    // Test I2C bus scan
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t scan_ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        
        if (scan_ret == ESP_OK) {
            ESP_LOGI(TAG, "Found I2C device at address 0x%02X", addr);
        }
    }
    ESP_LOGI(TAG, "I2C bus scan completed");
    
    return SLAVE_PCB_OK;
}

/**
 * @brief Set I2C multiplexer channel
 */
static slave_pcb_err_t set_i2c_mux_channel(uint8_t channel) {
    if (channel > 7) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // First try I2C-based multiplexer control (TCA9548A style)
    uint8_t channel_byte = 1 << channel; // Enable specific channel
    esp_err_t i2c_ret = i2c_master_write_to_device(I2C_MASTER_NUM, I2C_MULTIPLEXER_ADDR, 
                                                   &channel_byte, 1, pdMS_TO_TICKS(100));
    
    if (i2c_ret == ESP_OK) {
        ESP_LOGD(TAG, "I2C multiplexer channel %d selected (0x%02X)", channel, channel_byte);
        vTaskDelay(pdMS_TO_TICKS(5)); // Small delay for channel switching
        return SLAVE_PCB_OK;
    }
    
    // Fallback to GPIO-based multiplexer control
    ESP_LOGW(TAG, "I2C multiplexer control failed (%s), using GPIO fallback", esp_err_to_name(i2c_ret));
    gpio_set_level(I2C_MUX_A0, (channel & 0x01) ? 1 : 0);
    gpio_set_level(I2C_MUX_A1, (channel & 0x02) ? 1 : 0);
    gpio_set_level(I2C_MUX_A2, (channel & 0x04) ? 1 : 0);

    ESP_LOGD(TAG, "GPIO multiplexer pins: A0=%d, A1=%d, A2=%d", 
             (channel & 0x01) ? 1 : 0,
             (channel & 0x02) ? 1 : 0, 
             (channel & 0x04) ? 1 : 0);

    vTaskDelay(pdMS_TO_TICKS(5)); // Small delay to let the multiplexer settle
    return SLAVE_PCB_OK;
}

/**
 * @brief Read current from sensor via I2C
 */
static slave_pcb_err_t read_current_sensor(uint8_t channel, float *current_ma) {
    if (!current_ma) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Get shunt resistor value for this channel
    float shunt_resistor = get_shunt_resistor_for_channel(channel);
    if (shunt_resistor <= 0.0f) {
        ESP_LOGW(TAG, "Channel %d has no current sensor", channel);
        *current_ma = 0.0f;
        return SLAVE_PCB_OK;
    }

    slave_pcb_err_t ret = set_i2c_mux_channel(channel);
    if (ret != SLAVE_PCB_OK) {
        return ret;
    }

    // Calibrate INA219 for this specific shunt resistor
    ret = ina219_calibrate(shunt_resistor);
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGW(TAG, "INA219 calibration failed for channel %d", channel);
        *current_ma = 0.0f;
        return ret;
    }

    // Read current register from INA219
    uint16_t raw_current;
    ret = ina219_read_register(INA219_REG_CURRENT, &raw_current);
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGW(TAG, "INA219 current read failed for channel %d", channel);
        *current_ma = 0.0f;
        return ret;
    }

    // Convert to signed 16-bit value
    int16_t signed_current = (int16_t)raw_current;
    
    // Calculate current in mA using the same LSB as calibration
    float max_current_a = (shunt_resistor >= 0.09f) ? 2.0f : 4.0f; // Match calibration values
    float current_lsb = max_current_a / 4096.0f;  // Match calibration LSB calculation
    *current_ma = (float)signed_current * current_lsb * 1000.0f; // Convert to mA

    ESP_LOGD(TAG, "Channel %d current: %.1f mA (raw: 0x%04X=%d, shunt: %.3fΩ, LSB: %.3fmA)", 
             channel, *current_ma, raw_current, signed_current, shunt_resistor, current_lsb * 1000.0f);
    
    return SLAVE_PCB_OK;
}

/**
 * @brief Check if electrovalve is turning (has current draw)
 */
static bool is_electrovalve_turning(uint8_t electrovalve_id) {
    if (electrovalve_id >= 6) {
        return false;
    }

    float current_ma;
    slave_pcb_err_t ret = read_current_sensor(electrovalve_id, &current_ma);
    
    if (ret == SLAVE_PCB_OK) {
        electrovalve_states[electrovalve_id].last_current_reading = current_ma;
        electrovalve_states[electrovalve_id].is_turning = (current_ma > CURRENT_THRESHOLD_MA);
        return electrovalve_states[electrovalve_id].is_turning;
    }

    return false; // Assume not turning if we can't read current
}

/**
 * @brief Check if pump is pumping water (has current draw)
 */
static bool is_pump_pumping_water(uint8_t pump_id) {
    if (pump_id >= 4) {
        return false;
    }

    float current_ma;
    slave_pcb_err_t ret = read_current_sensor(pump_id + 5, &current_ma); // Pumps use channels 5-8
    
    if (ret == SLAVE_PCB_OK) {
        pump_states[pump_id].last_current_reading = current_ma;
        pump_states[pump_id].is_pumping = (current_ma > CURRENT_THRESHOLD_MA);
        return pump_states[pump_id].is_pumping;
    }

    return false; // Assume not pumping if we can't read current
}

/**
 * @brief Wait for electrovalves to reach target position
 */
static slave_pcb_err_t wait_for_electrovalves_ready(void) {
    ESP_LOGI(TAG, "Waiting for electrovalves to reach position");

    // For now, use a simple delay instead of current monitoring
    // This allows the system to work without current sensors being operational
    vTaskDelay(pdMS_TO_TICKS(15000)); // 20 seconds for electrovalves to turn
    
    ESP_LOGI(TAG, "Electrovalves should have reached position");
    return SLAVE_PCB_OK;

    /* DISABLED: Current-based monitoring 
    uint32_t start_time = esp_timer_get_time() / 1000;
    const uint32_t timeout_ms = 20000; // 20 second timeout

    while ((esp_timer_get_time() / 1000 - start_time) < timeout_ms) {
        bool all_ready = true;

        // Check all electrovalves
        for (int i = 0; i < 5; i++) {
            if (electrovalve_states[i].is_active) {
                if (is_electrovalve_turning(i)) {
                    all_ready = false;
                    ESP_LOGD(TAG, "Electrovalve %d still turning (%.1f mA)", 
                            i, electrovalve_states[i].last_current_reading);
                }
            }
        }

        if (all_ready) {
            ESP_LOGI(TAG, "All electrovalves ready");
            return SLAVE_PCB_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Check every 500ms
    }

    ESP_LOGE(TAG, "Timeout waiting for electrovalves to reach position");
    return SLAVE_PCB_ERR_TIMEOUT;
    */
}

/**
 * @brief Control electrovalve state
 */
static slave_pcb_err_t control_electrovalve(uint8_t electrovalve_id, bool state) {
    if (electrovalve_id >= 6) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    device_type_t device = DEVICE_ELECTROVALVE_A + electrovalve_id;
    slave_pcb_err_t ret = set_output_state(device, state);
    
    if (ret == SLAVE_PCB_OK) {
        electrovalve_states[electrovalve_id].is_active = state;
        electrovalve_states[electrovalve_id].target_state = state;
        electrovalve_states[electrovalve_id].last_state_change = esp_timer_get_time() / 1000;
        
        ESP_LOGI(TAG, "Electrovalve %d set to %s", electrovalve_id, state ? "ON" : "OFF");
    }

    return ret;
}

/**
 * @brief Control pump state
 */
static slave_pcb_err_t control_pump(uint8_t pump_id, bool state) {
    if (pump_id >= 4) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    device_type_t device = DEVICE_PUMP_PE + pump_id;
    slave_pcb_err_t ret = set_output_state(device, state);
    
    if (ret == SLAVE_PCB_OK) {
        pump_states[pump_id].is_active = state;
        pump_states[pump_id].target_state = state;
        pump_states[pump_id].last_state_change = esp_timer_get_time() / 1000;
        
        ESP_LOGI(TAG, "Pump %d set to %s", pump_id, state ? "ON" : "OFF");
    }

    return ret;
}

/**
 * @brief Apply case logic from main.c
 */
slave_pcb_err_t apply_electrovalve_pump_case(system_case_t case_id) {
    ESP_LOGI(TAG, "Applying case %s to electrovalves and pumps", get_case_string(case_id));

    if (case_id >= CASE_MAX) {
        ESP_LOGE(TAG, "Invalid case ID: %d", case_id);
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Case logic table (from main.c)
    typedef struct {
        bool electrovalve_a;
        bool electrovalve_b;
        bool electrovalve_c;
        bool electrovalve_d;
        bool electrovalve_e;
        bool pump_pe;
        bool pump_pd;
        bool pump_pv;
        bool pump_pp;
    } case_logic_t;

    // static const case_logic_t case_logic[CASE_MAX] = {
    //     [CASE_RST] = {0, 0, 0, 0, 0, 0, 0, 0, 0},
    //     [CASE_E1]  = {0, 1, 1, 0, 0, 1, 0, 0, 0},
    //     [CASE_E2]  = {0, 1, 0, 0, 0, 1, 0, 0, 0},
    //     [CASE_E3]  = {1, 1, 1, 0, 0, 1, 0, 0, 0},
    //     [CASE_E4]  = {1, 1, 0, 0, 0, 1, 0, 0, 0},
    //     [CASE_D1]  = {0, 0, 1, 1, 0, 1, 1, 0, 0},
    //     [CASE_D2]  = {0, 0, 0, 1, 0, 1, 1, 0, 0},
    //     [CASE_D3]  = {1, 0, 1, 1, 0, 1, 1, 0, 0},
    //     [CASE_D4]  = {1, 0, 0, 1, 0, 1, 1, 0, 0},
    //     [CASE_V1]  = {0, 0, 1, 0, 1, 0, 0, 1, 0},
    //     [CASE_V2]  = {0, 0, 0, 0, 1, 0, 0, 1, 0},
    //     [CASE_P1]  = {0, 0, 0, 0, 1, 0, 0, 0, 1}
    // };
    /*
    | Cas | A | B | C | D | E | PE | PD | PV | PP |
    |-----|---|---|---|---|---|----|----|----|----| 
    | RST | 0 | 0 | 0 | 0 | 0 |  0 |  0 |  0 |  0 | 
    | E1  | 0 | 1 | 1 | 0 | 0 |  1 |  0 |  0 |  0 | 
    | E2  | 0 | 1 | 0 | 0 | 0 |  1 |  0 |  0 |  0 | 
    | E3  | 1 | 1 | 1 | 0 | 0 |  1 |  0 |  0 |  0 | 
    | E4  | 1 | 1 | 0 | 0 | 0 |  1 |  0 |  0 |  0 | 
    | D1  | 0 | 0 | 1 | 1 | 0 |  1 |  1 |  0 |  0 | 
    | D2  | 0 | 0 | 0 | 1 | 0 |  1 |  1 |  0 |  0 | 
    | D3  | 1 | 0 | 1 | 1 | 0 |  1 |  1 |  0 |  0 | 
    | D4  | 1 | 0 | 0 | 1 | 0 |  1 |  1 |  0 |  0 | 
    | V1  | 0 | 0 | 1 | 0 | 1 |  0 |  0 |  1 |  0 | 
    | V2  | 0 | 0 | 0 | 0 | 1 |  0 |  0 |  1 |  0 | 
    | P1  |0-1| 0 | 0 | 0 | 1 |  0 |  0 |  0 |  1 |
    */
    static const case_logic_t case_logic[CASE_MAX] = {
        [CASE_RST] = {0, 0, 0, 0, 0, 0, 0, 0, 0},
        [CASE_E1]  = {1, 1, 1, 0, 0, 1, 0, 0, 0},
        [CASE_E2]  = {1, 1, 0, 0, 0, 1, 0, 0, 0},
        [CASE_E3]  = {0, 1, 1, 0, 0, 1, 0, 0, 0},
        [CASE_E4]  = {0, 1, 0, 0, 0, 1, 0, 0, 0},
        [CASE_D1]  = {1, 0, 1, 1, 0, 1, 1, 0, 0},
        [CASE_D2]  = {1, 0, 0, 1, 0, 1, 1, 0, 0},
        [CASE_D3]  = {0, 0, 1, 1, 0, 1, 1, 0, 0},
        [CASE_D4]  = {0, 0, 0, 1, 0, 1, 1, 0, 0},
        [CASE_V1]  = {1, 0, 1, 0, 1, 0, 0, 1, 0},
        [CASE_V2]  = {1, 0, 0, 0, 1, 0, 0, 1, 0},
        [CASE_P1]  = {1, 0, 0, 0, 1, 0, 0, 0, 1}
    };

    const case_logic_t *logic = &case_logic[case_id];
    slave_pcb_err_t ret = SLAVE_PCB_OK;

    // Step 1: Set electrovalves first
    ESP_LOGI(TAG, "Setting electrovalves for case %s", get_case_string(case_id));
    ret |= control_electrovalve(0, logic->electrovalve_a);
    ret |= control_electrovalve(1, logic->electrovalve_b);
    ret |= control_electrovalve(2, logic->electrovalve_c);
    ret |= control_electrovalve(3, logic->electrovalve_d);
    ret |= control_electrovalve(4, logic->electrovalve_e);

    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to set electrovalves for case %s", get_case_string(case_id));
        return ret;
    }

    // Step 2: Wait for electrovalves to reach target position
    ESP_LOGI(TAG, "Waiting for electrovalves to reach position...");
    ret = wait_for_electrovalves_ready();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Electrovalves did not reach position in time for case %s", get_case_string(case_id));
        return ret;
    }

    // Step 3: Set pumps
    ESP_LOGI(TAG, "Setting pumps for case %s", get_case_string(case_id));
    ret |= control_pump(0, logic->pump_pe);
    ret |= control_pump(1, logic->pump_pd);
    ret |= control_pump(2, logic->pump_pv);
    ret |= control_pump(3, logic->pump_pp);

    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to set pumps for case %s", get_case_string(case_id));
        return ret;
    }

    ESP_LOGI(TAG, "Successfully applied case %s", get_case_string(case_id));

    // Step 4: Notify button manager that transition is complete
    ESP_LOGI(TAG, "Notifying button manager that transition is complete");
    button_manager_notify_transition_complete();
    ESP_LOGI(TAG, "Button manager notification sent");

    return SLAVE_PCB_OK;
}

/**
 * @brief Monitor pump health and send notifications
 */
static void monitor_pump_health(void) {
    // DISABLED: Current-based health monitoring for now
    // The system will work without current sensor validation
    return;
    
    /* DISABLED: Current-based monitoring
    static uint32_t last_check = 0;
    uint32_t now = esp_timer_get_time() / 1000;

    if (now - last_check < 1000) { // Check every second
        return;
    }
    last_check = now;

    // Check each active pump
    for (int i = 0; i < 4; i++) {
        if (pump_states[i].is_active) {
            bool is_pumping = is_pump_pumping_water(i);
            
            if (!is_pumping) {
                ESP_LOGW(TAG, "Pump %d is not pumping water (current: %.1f mA)", 
                        i, pump_states[i].last_current_reading);
                
                // Send notification to communication manager
                comm_msg_t msg = {
                    .type = MSG_ERROR,
                    .timestamp = now,
                    .data.error_data = {
                        .error_code = SLAVE_PCB_ERR_DEVICE_NOT_FOUND,
                    }
                };
                snprintf(msg.data.error_data.description, sizeof(msg.data.error_data.description),
                        "Pump %d not pumping water", i);
                
                xQueueSend(comm_queue, &msg, 0);

                // For PE pump, switch to RST case if it fails
                if (i == 0) { // PE pump
                    ESP_LOGE(TAG, "PE pump failure, switching to RST case");
                    comm_msg_t rst_msg = {
                        .type = MSG_CASE_CHANGE,
                        .timestamp = now,
                        .data.case_data = CASE_RST
                    };
                    xQueueSend(comm_queue, &rst_msg, 0);
                }
            }
        }
    }
    */
}

/**
 * @brief Electrovalve and Pump Manager initialization
 */
slave_pcb_err_t electrovalve_pump_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Electrovalve and Pump Manager");

    // Initialize I2C multiplexer GPIO pins
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << I2C_MUX_A0) | (1ULL << I2C_MUX_A1) | (1ULL << I2C_MUX_A2),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    esp_err_t gpio_ret = gpio_config(&io_conf);
    if (gpio_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C multiplexer GPIOs: %s", esp_err_to_name(gpio_ret));
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Set initial multiplexer state (channel 0)
    gpio_set_level(I2C_MUX_A0, 0);
    gpio_set_level(I2C_MUX_A1, 0);
    gpio_set_level(I2C_MUX_A2, 0);

    ESP_LOGI(TAG, "I2C multiplexer GPIOs initialized: A0=%d, A1=%d, A2=%d", I2C_MUX_A0, I2C_MUX_A1, I2C_MUX_A2);

    slave_pcb_err_t ret = init_i2c();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C");
        return ret;
    }

    // Initialize device states
    memset(electrovalve_states, 0, sizeof(electrovalve_states));
    memset(pump_states, 0, sizeof(pump_states));

    // Set all devices to off state initially
    for (int i = 0; i < 5; i++) {
        control_electrovalve(i, false);
    }
    for (int i = 0; i < 4; i++) {
        control_pump(i, false);
    }

    ESP_LOGI(TAG, "Electrovalve and Pump Manager initialized successfully");
    return SLAVE_PCB_OK;
}

/**
 * @brief Electrovalve and Pump Manager main task
 */
void electrovalve_pump_manager_task(void *pvParameters) {
    ESP_LOGI(TAG, "Electrovalve and Pump Manager task started");

    while (1) {
        // Monitor current status of all devices
        for (int i = 0; i < 5; i++) {
            if (electrovalve_states[i].is_active) {
                is_electrovalve_turning(i);
            }
        }

        // Monitor pump health
        monitor_pump_health();

        // Send status updates
        static uint32_t last_status_update = 0;
        uint32_t now = esp_timer_get_time() / 1000;
        
        if (now - last_status_update > 5000) { // Every 5 seconds
            last_status_update = now;
            
            // Send device status for each active device
            for (int i = 0; i < 5; i++) {
                if (electrovalve_states[i].is_active) {
                    comm_msg_t msg = {
                        .type = MSG_DEVICE_STATUS,
                        .timestamp = now,
                        .data.device_status = {
                            .device = DEVICE_ELECTROVALVE_A + i,
                            .status = !electrovalve_states[i].is_turning
                        }
                    };
                    xQueueSend(comm_queue, &msg, 0);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Get current reading for a specific pump (for simulation/testing)
 */
float get_pump_current_reading(uint8_t pump_id) {
    if (pump_id >= 4) {
        ESP_LOGE(TAG, "Invalid pump ID: %d", pump_id);
        return 0.0f;
    }

    // Map pump IDs to I2C channels
    // PE=0 -> channel 5, PD=1 -> channel 6, PV=2 -> channel 7, PP=3 -> channel 8
    uint8_t channel = pump_id + 5;
    
    const char* pump_names[] = {"PE", "PD", "PV", "PP"};
    
    // Check if this pump has a current sensor
    float shunt_resistor = get_shunt_resistor_for_channel(channel);
    if (shunt_resistor <= 0.0f) {
        ESP_LOGI(TAG, "Pump %s (ID %d) has no current sensor", pump_names[pump_id], pump_id);
        pump_states[pump_id].last_current_reading = 0.0f;
        return 0.0f;
    }

    float current_ma;
    slave_pcb_err_t ret = read_current_sensor(channel, &current_ma);
    
    if (ret == SLAVE_PCB_OK) {
        pump_states[pump_id].last_current_reading = current_ma;
        ESP_LOGI(TAG, "Pump %s (ID %d) current reading: %.1f mA (shunt: %.0fmΩ)", 
                 pump_names[pump_id], pump_id, current_ma, shunt_resistor * 1000);
        return current_ma;
    } else {
        ESP_LOGW(TAG, "Failed to read current for pump %s (ID %d)", pump_names[pump_id], pump_id);
        return 0.0f;
    }
}

/**
 * @brief Get current reading for a specific electrovalve (for simulation/testing)
 */
float get_electrovalve_current_reading(uint8_t electrovalve_id) {
    if (electrovalve_id >= 6) {
        ESP_LOGE(TAG, "Invalid electrovalve ID: %d", electrovalve_id);
        return 0.0f;
    }

    float current_ma;
    slave_pcb_err_t ret = read_current_sensor(electrovalve_id, &current_ma);
    
    if (ret == SLAVE_PCB_OK) {
        if (electrovalve_id < 5) {
            electrovalve_states[electrovalve_id].last_current_reading = current_ma;
        }
        ESP_LOGI(TAG, "Electrovalve %c (ID %d) current reading: %.1f mA (shunt: 100mΩ)", 
                 'A' + electrovalve_id, electrovalve_id, current_ma);
        return current_ma;
    } else {
        ESP_LOGW(TAG, "Failed to read current for electrovalve %c (ID %d)", 
                 'A' + electrovalve_id, electrovalve_id);
        return 0.0f;
    }
}
