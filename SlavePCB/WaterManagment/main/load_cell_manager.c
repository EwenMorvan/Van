#include "slave_pcb.h"

static const char *TAG = "LOADCELL_MGR";

// HX711 configuration
#define HX711_STABILIZING_TIME_MS 100
#define HX711_TIMEOUT_MS 1000
#define HX711_SAMPLES_FOR_AVERAGE 10

// Tank configuration
typedef struct {
    int dt_pin;
    float calibration_factor;
    float tare_offset;
    float last_weight;
    uint32_t last_reading_time;
    bool is_stable;
} tank_config_t;

static tank_config_t tank_configs[TANK_MAX] = {
    [TANK_A] = {HX_711_DT_A, 1000.0f, 0.0f, 0.0f, 0, false},
    [TANK_B] = {HX_711_DT_B, 1000.0f, 0.0f, 0.0f, 0, false},
    [TANK_C] = {HX_711_DT_C, 1000.0f, 0.0f, 0.0f, 0, false},
    [TANK_D] = {HX_711_DT_D, 1000.0f, 0.0f, 0.0f, 0, false},
    [TANK_E] = {HX_711_DT_E, 1000.0f, 0.0f, 0.0f, 0, false}
};

// System state thresholds (in kg)
#define CLEAN_WATER_EMPTY_THRESHOLD    5.0f   // CE threshold
#define DIRTY_WATER_FULL_THRESHOLD     80.0f  // DF threshold  
#define DIRTY_WATER_EMPTY_THRESHOLD    2.0f   // DE threshold
#define RECYCLED_WATER_FULL_THRESHOLD  80.0f  // RF threshold
#define RECYCLED_WATER_EMPTY_THRESHOLD 5.0f   // RE threshold

// Current system states
static volatile uint32_t current_system_states = 0;

/**
 * @brief Wait for HX711 to be ready
 */
static bool wait_for_hx711_ready(int dt_pin, uint32_t timeout_ms) {
    uint32_t start_time = esp_timer_get_time() / 1000;
    
    while ((esp_timer_get_time() / 1000 - start_time) < timeout_ms) {
        if (gpio_get_level(dt_pin) == 0) { // HX711 ready when DT is low
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    return false;
}

/**
 * @brief Read raw value from HX711
 */
static bool read_hx711_raw(int dt_pin, uint32_t *raw_value) {
    if (!raw_value) {
        return false;
    }

    // Wait for HX711 to be ready
    if (!wait_for_hx711_ready(dt_pin, HX711_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "HX711 on pin %d not ready", dt_pin);
        return false;
    }

    uint32_t value = 0;
    
    // Disable interrupts during critical timing section
    portDISABLE_INTERRUPTS();

    // Read 24 bits of data
    for (int i = 0; i < 24; i++) {
        gpio_set_level(HX_711_SCK, 1);
        vTaskDelay(1); // Small delay for clock high
        value = (value << 1) | gpio_get_level(dt_pin);
        gpio_set_level(HX_711_SCK, 0);
        vTaskDelay(1); // Small delay for clock low
    }

    // Set gain for next reading (1 additional clock pulse = gain 128, channel A)
    gpio_set_level(HX_711_SCK, 1);
    vTaskDelay(1);
    gpio_set_level(HX_711_SCK, 0);

    portENABLE_INTERRUPTS();

    // Convert to signed value (24-bit two's complement)
    if (value & 0x800000) {
        value |= 0xFF000000; // Sign extend
    }

    *raw_value = value;
    return true;
}

/**
 * @brief Read weight from specific tank
 */
static slave_pcb_err_t read_tank_weight(tank_id_t tank_id, float *weight) {
    if (tank_id >= TANK_MAX || !weight) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    tank_config_t *config = &tank_configs[tank_id];
    uint32_t raw_values[HX711_SAMPLES_FOR_AVERAGE];
    int valid_samples = 0;

    ESP_LOGD(TAG, "Reading weight from tank %d", tank_id);

    // Take multiple samples for averaging
    for (int i = 0; i < HX711_SAMPLES_FOR_AVERAGE; i++) {
        uint32_t raw_value;
        if (read_hx711_raw(config->dt_pin, &raw_value)) {
            raw_values[valid_samples++] = raw_value;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between samples
    }

    if (valid_samples == 0) {
        ESP_LOGE(TAG, "Failed to read any valid samples from tank %d", tank_id);
        return SLAVE_PCB_ERR_TIMEOUT;
    }

    // Calculate average
    uint64_t sum = 0;
    for (int i = 0; i < valid_samples; i++) {
        sum += raw_values[i];
    }
    uint32_t average_raw = sum / valid_samples;

    // Convert to weight (kg)
    float raw_weight = (float)((int32_t)average_raw - config->tare_offset) / config->calibration_factor;
    
    // Apply simple filtering (basic low-pass filter)
    if (config->last_reading_time > 0) {
        float alpha = 0.1f; // Filter coefficient
        *weight = alpha * raw_weight + (1.0f - alpha) * config->last_weight;
    } else {
        *weight = raw_weight;
    }

    config->last_weight = *weight;
    config->last_reading_time = esp_timer_get_time() / 1000;

    ESP_LOGD(TAG, "Tank %d weight: %.2f kg (raw: %lu, samples: %d)", 
             tank_id, *weight, average_raw, valid_samples);

    return SLAVE_PCB_OK;
}

/**
 * @brief Read all HX711 modules sequentially
 */
static slave_pcb_err_t read_all_tanks(void) {
    ESP_LOGD(TAG, "Reading all tank weights");

    for (tank_id_t tank = 0; tank < TANK_MAX; tank++) {
        float weight;
        slave_pcb_err_t ret = read_tank_weight(tank, &weight);
        
        if (ret == SLAVE_PCB_OK) {
            // Send weight data to communication manager
            comm_msg_t msg = {
                .type = MSG_LOAD_CELL_DATA,
                .timestamp = esp_timer_get_time() / 1000,
                .data.load_cell_data = {
                    .tank = tank,
                    .weight = weight
                }
            };
            
            xQueueSend(loadcell_queue, &msg, 0);
        } else {
            ESP_LOGW(TAG, "Failed to read tank %d: %s", tank, get_error_string(ret));
        }
    }

    return SLAVE_PCB_OK;
}

/**
 * @brief Update system states based on tank weights
 */
static void update_system_states(void) {
    uint32_t new_states = 0;

    // Example tank assignments (this would be configured based on actual tank setup)
    float clean_water_weight = tank_configs[TANK_A].last_weight; // Assume tank A is clean water
    float dirty_water_weight = tank_configs[TANK_B].last_weight; // Assume tank B is dirty water  
    float recycled_water_weight = tank_configs[TANK_C].last_weight; // Assume tank C is recycled water

    // Check thresholds and set state flags
    if (clean_water_weight < CLEAN_WATER_EMPTY_THRESHOLD) {
        new_states |= STATE_CE;
    }
    
    if (dirty_water_weight > DIRTY_WATER_FULL_THRESHOLD) {
        new_states |= STATE_DF;
    }
    
    if (dirty_water_weight < DIRTY_WATER_EMPTY_THRESHOLD) {
        new_states |= STATE_DE;
    }
    
    if (recycled_water_weight > RECYCLED_WATER_FULL_THRESHOLD) {
        new_states |= STATE_RF;
    }
    
    if (recycled_water_weight < RECYCLED_WATER_EMPTY_THRESHOLD) {
        new_states |= STATE_RE;
    }

    // Check for state changes
    if (new_states != current_system_states) {
        ESP_LOGI(TAG, "System states changed: 0x%lx -> 0x%lx", current_system_states, new_states);
        
        if (new_states & STATE_CE) ESP_LOGW(TAG, "Clean water tank is empty!");
        if (new_states & STATE_DF) ESP_LOGW(TAG, "Dirty water tank is full!");
        if (new_states & STATE_DE) ESP_LOGW(TAG, "Dirty water tank is empty!");
        if (new_states & STATE_RF) ESP_LOGW(TAG, "Recycled water tank is full!");
        if (new_states & STATE_RE) ESP_LOGW(TAG, "Recycled water tank is empty!");

        current_system_states = new_states;

        // Send state change notification
        comm_msg_t msg = {
            .type = MSG_ERROR,
            .timestamp = esp_timer_get_time() / 1000,
            .data.error_data = {
                .error_code = SLAVE_PCB_OK,
            }
        };
        snprintf(msg.data.error_data.description, sizeof(msg.data.error_data.description),
                "System states changed: 0x%lx", new_states);
        
        xQueueSend(comm_queue, &msg, 0);
    }
}

/**
 * @brief Calibrate load cell (set tare)
 */
static slave_pcb_err_t calibrate_tank_tare(tank_id_t tank_id) {
    if (tank_id >= TANK_MAX) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Calibrating tare for tank %d", tank_id);

    tank_config_t *config = &tank_configs[tank_id];
    uint32_t raw_sum = 0;
    int valid_readings = 0;

    // Take multiple readings for stable tare
    for (int i = 0; i < 20; i++) {
        uint32_t raw_value;
        if (read_hx711_raw(config->dt_pin, &raw_value)) {
            raw_sum += raw_value;
            valid_readings++;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (valid_readings < 10) {
        ESP_LOGE(TAG, "Not enough valid readings for tare calibration");
        return SLAVE_PCB_ERR_TIMEOUT;
    }

    config->tare_offset = raw_sum / valid_readings;
    ESP_LOGI(TAG, "Tank %d tare offset set to: %.0f", tank_id, config->tare_offset);

    return SLAVE_PCB_OK;
}

/**
 * @brief Get current system states
 */
uint32_t get_current_system_states(void) {
    return current_system_states;
}

/**
 * @brief Load Cell Manager initialization
 */
slave_pcb_err_t load_cell_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Load Cell Manager");

    // Initialize HX711 SCK pin
    gpio_set_level(HX_711_SCK, 0);

    // Wait for HX711 modules to stabilize
    ESP_LOGI(TAG, "Waiting for HX711 modules to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(HX711_STABILIZING_TIME_MS));

    // Optionally calibrate tare for all tanks
    ESP_LOGI(TAG, "Performing tare calibration...");
    for (tank_id_t tank = 0; tank < TANK_MAX; tank++) {
        slave_pcb_err_t ret = calibrate_tank_tare(tank);
        if (ret != SLAVE_PCB_OK) {
            ESP_LOGW(TAG, "Failed to calibrate tare for tank %d: %s", 
                     tank, get_error_string(ret));
        }
    }

    ESP_LOGI(TAG, "Load Cell Manager initialized successfully");
    return SLAVE_PCB_OK;
}

/**
 * @brief Load Cell Manager main task
 */
void load_cell_manager_task(void *pvParameters) {
    ESP_LOGI(TAG, "Load Cell Manager task started");

    const TickType_t reading_interval = pdMS_TO_TICKS(2000); // Read every 2 seconds
    TickType_t last_wake_time = xTaskGetTickCount();

    while (1) {
        // Read all tank weights sequentially
        slave_pcb_err_t ret = read_all_tanks();
        if (ret != SLAVE_PCB_OK) {
            ESP_LOGW(TAG, "Error reading tanks: %s", get_error_string(ret));
        }

        // Update system states based on weights
        update_system_states();

        // Log current weights periodically
        static uint32_t last_log_time = 0;
        uint32_t now = esp_timer_get_time() / 1000;
        
        if (now - last_log_time > 10000) { // Every 10 seconds
            last_log_time = now;
            ESP_LOGI(TAG, "Tank weights - A:%.1f B:%.1f C:%.1f D:%.1f E:%.1f kg",
                     tank_configs[TANK_A].last_weight,
                     tank_configs[TANK_B].last_weight,
                     tank_configs[TANK_C].last_weight,
                     tank_configs[TANK_D].last_weight,
                     tank_configs[TANK_E].last_weight);
            ESP_LOGI(TAG, "System states: 0x%lx", current_system_states);
        }

        // Wait for next reading interval
        vTaskDelayUntil(&last_wake_time, reading_interval);
    }
}
