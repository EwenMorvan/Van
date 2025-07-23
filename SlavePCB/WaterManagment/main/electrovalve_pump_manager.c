#include "slave_pcb.h"

static const char *TAG = "EV_PUMP_MGR";

// I2C configuration
#define I2C_MASTER_SCL_IO    I2C_MUX_SCL
#define I2C_MASTER_SDA_IO    I2C_MUX_SDA
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

// Current sensor I2C address (example)
#define CURRENT_SENSOR_ADDR  0x40

// Current threshold for detecting if electrovalve/pump is active
#define CURRENT_THRESHOLD_MA 100

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

/**
 * @brief Initialize I2C for current sensors
 */
static slave_pcb_err_t init_i2c(void) {
    ESP_LOGI(TAG, "Initializing I2C");

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
    return SLAVE_PCB_OK;
}

/**
 * @brief Set I2C multiplexer channel
 */
static slave_pcb_err_t set_i2c_mux_channel(uint8_t channel) {
    if (channel > 7) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Set TCA multiplexer address pins
    gpio_set_level(I2C_MUX_A0, (channel & 0x01) ? 1 : 0);
    gpio_set_level(I2C_MUX_A1, (channel & 0x02) ? 1 : 0);
    gpio_set_level(I2C_MUX_A2, (channel & 0x04) ? 1 : 0);

    // Small delay to let the multiplexer settle
    vTaskDelay(pdMS_TO_TICKS(1));

    return SLAVE_PCB_OK;
}

/**
 * @brief Read current from sensor via I2C
 */
static slave_pcb_err_t read_current_sensor(uint8_t channel, float *current_ma) {
    if (!current_ma) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    slave_pcb_err_t ret = set_i2c_mux_channel(channel);
    if (ret != SLAVE_PCB_OK) {
        return ret;
    }

    uint8_t data[2];
    esp_err_t i2c_ret = i2c_master_read_from_device(I2C_MASTER_NUM, CURRENT_SENSOR_ADDR, 
                                                   data, 2, pdMS_TO_TICKS(100));
    if (i2c_ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C read failed for channel %d: %s", channel, esp_err_to_name(i2c_ret));
        *current_ma = 0.0f;
        return SLAVE_PCB_ERR_I2C_FAIL;
    }

    // Convert raw ADC value to current (this depends on the actual sensor used)
    uint16_t raw_value = (data[0] << 8) | data[1];
    *current_ma = (float)raw_value * 0.1f; // Example conversion factor

    ESP_LOGD(TAG, "Channel %d current: %.1f mA", channel, *current_ma);
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

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGE(TAG, "Timeout waiting for electrovalves to reach position");
    return SLAVE_PCB_ERR_TIMEOUT;
}

/**
 * @brief Control electrovalve state
 */
static slave_pcb_err_t control_electrovalve(uint8_t electrovalve_id, bool state) {
    if (electrovalve_id >= 5) {
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

    // This function would be called from the main coordinator
    // For now, it's a placeholder that shows how the logic would be applied

    // Example logic for CASE_E1 (Evier EP -> ES)
    if (case_id == CASE_E1) {
        control_electrovalve(0, false); // A = 0
        control_electrovalve(1, true);  // B = 1
        control_electrovalve(2, true);  // C = 1
        control_electrovalve(3, false); // D = 0
        control_electrovalve(4, false); // E = 0

        // Wait for electrovalves
        slave_pcb_err_t ret = wait_for_electrovalves_ready();
        if (ret != SLAVE_PCB_OK) {
            return ret;
        }

        // Activate pumps
        control_pump(0, true);  // PE = 1
        control_pump(1, false); // PD = 0
        control_pump(2, false); // PV = 0
        control_pump(3, false); // PP = 0
    }

    return SLAVE_PCB_OK;
}

/**
 * @brief Monitor pump health and send notifications
 */
static void monitor_pump_health(void) {
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
}

/**
 * @brief Electrovalve and Pump Manager initialization
 */
slave_pcb_err_t electrovalve_pump_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Electrovalve and Pump Manager");

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
