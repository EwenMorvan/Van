#include "slave_pcb.h"
#include "esp_rom_sys.h"  // For esp_rom_delay_us
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h>  // For fabs

static const char *TAG = "LOADCELL_MGR";

// NVS namespace for calibration data
#define NVS_NAMESPACE "loadcell_cal"

// HX711 configuration - optimized for speed and stability
#define HX711_STABILIZING_TIME_MS 100   // Reduced from 200ms
#define HX711_TIMEOUT_MS 500            // Reduced timeout
#define HX711_SAMPLES_FOR_AVERAGE 15    // More samples for better stability
#define HX711_INTER_MODULE_DELAY_MS 20  // Reduced delay between modules
#define HX711_CLOCK_DELAY_US 1          // Faster clock timing
#define HX711_SAMPLE_DELAY_MS 5         // Short delay between individual samples
#define HX711_CALIBRATION_SAMPLES 50    // More samples for calibration
#define HX711_MOVING_AVERAGE_SIZE 10    // Moving average filter size

// Advanced filtering parameters
#define OUTLIER_THRESHOLD_PERCENT 10    // Remove samples more than 10% from median
#define STABILITY_THRESHOLD 0.1f        // Weight change threshold for stability (kg)
#define STABILITY_COUNT_REQUIRED 5      // Number of stable readings required

// Tank configuration with advanced filtering
typedef struct {
    int dt_pin;
    float calibration_factor;
    float tare_offset;
    float last_weight;
    uint32_t last_reading_time;
    bool is_stable;
    
    // Moving average filter
    float moving_avg_buffer[HX711_MOVING_AVERAGE_SIZE];
    uint8_t moving_avg_index;
    uint8_t moving_avg_count;
    float moving_avg_sum;
    
    // Stability tracking
    uint8_t stable_count;
    float last_stable_weight;
} tank_config_t;

static tank_config_t tank_configs[TANK_MAX] = {
    [TANK_A] = {HX_711_DT_A, 1000.0f, 0.0f, 0.0f, 0, false, {0}, 0, 0, 0.0f, 0, 0.0f},
    [TANK_B] = {HX_711_DT_B, 1000.0f, 0.0f, 0.0f, 0, false, {0}, 0, 0, 0.0f, 0, 0.0f},
    [TANK_C] = {HX_711_DT_C, 1000.0f, 0.0f, 0.0f, 0, false, {0}, 0, 0, 0.0f, 0, 0.0f},
    [TANK_D] = {HX_711_DT_D, 1000.0f, 0.0f, 0.0f, 0, false, {0}, 0, 0, 0.0f, 0, 0.0f},
    [TANK_E] = {HX_711_DT_E, 1000.0f, 0.0f, 0.0f, 0, false, {0}, 0, 0, 0.0f, 0, 0.0f}
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
 * @brief Reset all HX711 modules to ensure synchronized state
 */
static void reset_all_hx711_modules(void) {
    // ESP_LOGI(TAG, "Resetting all HX711 modules for synchronization");
    
    // Pull SCK high for >60us to reset all HX711 modules
    gpio_set_level(HX_711_SCK, 1);
    esp_rom_delay_us(100); // 100us delay to ensure reset
    gpio_set_level(HX_711_SCK, 0);
    
    // Wait for modules to stabilize
    vTaskDelay(pdMS_TO_TICKS(HX711_STABILIZING_TIME_MS));
}

/**
 * @brief Wait for HX711 to be ready (optimized)
 */
static bool wait_for_hx711_ready(int dt_pin, uint32_t timeout_ms) {
    uint32_t start_time = esp_timer_get_time() / 1000;
    
    // First, do a quick check without delay
    if (gpio_get_level(dt_pin) == 0) {
        return true;
    }
    
    // If not ready, wait with minimal delays
    while ((esp_timer_get_time() / 1000 - start_time) < timeout_ms) {
        if (gpio_get_level(dt_pin) == 0) {
            return true;
        }
        esp_rom_delay_us(100); // Microsecond delay instead of millisecond
    }
    
    return false;
}

/**
 * @brief Read raw value from HX711 with optimized timing
 */
static bool read_hx711_raw(int dt_pin, uint32_t *raw_value) {
    if (!raw_value) {
        return false;
    }

    // Ensure SCK is low before starting
    gpio_set_level(HX_711_SCK, 0);
    esp_rom_delay_us(2); // Reduced delay

    // Wait for HX711 to be ready
    if (!wait_for_hx711_ready(dt_pin, HX711_TIMEOUT_MS)) {
        return false; // Removed warning to reduce log spam
    }

    uint32_t value = 0;
    
    // Disable interrupts during critical timing section
    portDISABLE_INTERRUPTS();

    // Read 24 bits of data with optimized timing
    for (int i = 0; i < 24; i++) {
        gpio_set_level(HX_711_SCK, 1);
        esp_rom_delay_us(HX711_CLOCK_DELAY_US);
        value = (value << 1) | gpio_get_level(dt_pin);
        gpio_set_level(HX_711_SCK, 0);
        esp_rom_delay_us(HX711_CLOCK_DELAY_US);
    }

    // Set gain for next reading (1 additional clock pulse = gain 128, channel A)
    gpio_set_level(HX_711_SCK, 1);
    esp_rom_delay_us(HX711_CLOCK_DELAY_US);
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
 * @brief Add value to moving average filter
 */
static void update_moving_average(tank_config_t *config, float new_value) {
    // Remove old value from sum if buffer is full
    if (config->moving_avg_count == HX711_MOVING_AVERAGE_SIZE) {
        config->moving_avg_sum -= config->moving_avg_buffer[config->moving_avg_index];
    } else {
        config->moving_avg_count++;
    }
    
    // Add new value
    config->moving_avg_buffer[config->moving_avg_index] = new_value;
    config->moving_avg_sum += new_value;
    
    // Update index (circular buffer)
    config->moving_avg_index = (config->moving_avg_index + 1) % HX711_MOVING_AVERAGE_SIZE;
}

/**
 * @brief Get filtered weight from moving average
 */
static float get_filtered_weight(tank_config_t *config) {
    if (config->moving_avg_count == 0) {
        return 0.0f;
    }
    return config->moving_avg_sum / config->moving_avg_count;
}

/**
 * @brief Advanced outlier detection and removal
 */
static int remove_outliers(uint32_t *values, int count, uint32_t *output) {
    if (count < 3) {
        // Not enough samples for outlier detection, copy all
        for (int i = 0; i < count; i++) {
            output[i] = values[i];
        }
        return count;
    }
    
    // Calculate median
    uint32_t sorted[HX711_SAMPLES_FOR_AVERAGE];
    for (int i = 0; i < count; i++) {
        sorted[i] = values[i];
    }
    
    // Simple bubble sort for small arrays
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                uint32_t temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    uint32_t median = sorted[count / 2];
    uint32_t threshold = median * OUTLIER_THRESHOLD_PERCENT / 100;
    
    // Keep values within threshold of median
    int output_count = 0;
    for (int i = 0; i < count; i++) {
        uint32_t diff = (values[i] > median) ? (values[i] - median) : (median - values[i]);
        if (diff <= threshold) {
            output[output_count++] = values[i];
        }
    }
    
    // Ensure we have at least half the original samples
    if (output_count < count / 2) {
        // Fallback: use all samples
        for (int i = 0; i < count; i++) {
            output[i] = values[i];
        }
        return count;
    }
    
    return output_count;
}

/**
 * @brief Read weight from specific tank with advanced filtering
 */
static slave_pcb_err_t read_tank_weight(tank_id_t tank_id, float *weight) {
    if (tank_id >= TANK_MAX || !weight) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    tank_config_t *config = &tank_configs[tank_id];
    uint32_t raw_values[HX711_SAMPLES_FOR_AVERAGE];
    int valid_samples = 0;

    // Take multiple samples quickly
    for (int i = 0; i < HX711_SAMPLES_FOR_AVERAGE; i++) {
        uint32_t raw_value;
        if (read_hx711_raw(config->dt_pin, &raw_value)) {
            raw_values[valid_samples++] = raw_value;
        }
        // Short delay between samples for speed
        if (i < HX711_SAMPLES_FOR_AVERAGE - 1) {
            vTaskDelay(pdMS_TO_TICKS(HX711_SAMPLE_DELAY_MS));
        }
    }

    if (valid_samples < HX711_SAMPLES_FOR_AVERAGE / 2) {
        ESP_LOGW(TAG, "Insufficient samples from tank %d (%d/%d)", 
                 tank_id, valid_samples, HX711_SAMPLES_FOR_AVERAGE);
        return SLAVE_PCB_ERR_TIMEOUT;
    }

    // Remove outliers
    uint32_t filtered_values[HX711_SAMPLES_FOR_AVERAGE];
    int filtered_count = remove_outliers(raw_values, valid_samples, filtered_values);

    // Calculate average of filtered values
    uint64_t sum = 0;
    for (int i = 0; i < filtered_count; i++) {
        sum += filtered_values[i];
    }
    uint32_t average_raw = sum / filtered_count;

    // Convert to weight (kg)
    float raw_weight = (float)((int32_t)average_raw - (int32_t)config->tare_offset) / config->calibration_factor;
    
    // Update moving average filter
    update_moving_average(config, raw_weight);
    
    // Get filtered weight
    float filtered_weight = get_filtered_weight(config);
    
    // Update stability tracking
    if (config->moving_avg_count >= HX711_MOVING_AVERAGE_SIZE / 2) {
        float weight_diff = fabs(filtered_weight - config->last_stable_weight);
        if (weight_diff < STABILITY_THRESHOLD) {
            config->stable_count++;
        } else {
            config->stable_count = 0;
            config->last_stable_weight = filtered_weight;
        }
        
        config->is_stable = (config->stable_count >= STABILITY_COUNT_REQUIRED);
    }

    *weight = filtered_weight;
    config->last_weight = *weight;
    config->last_reading_time = esp_timer_get_time() / 1000;

    ESP_LOGD(TAG, "Tank %d: raw=%lu, weight=%.2f kg, filtered=%.2f kg, stable=%s (samples=%d, outliers=%d)", 
             tank_id, average_raw, raw_weight, filtered_weight, 
             config->is_stable ? "YES" : "NO", valid_samples, valid_samples - filtered_count);

    return SLAVE_PCB_OK;
}

/**
 * @brief Read all HX711 modules sequentially - SAFE VERSION
 */
static slave_pcb_err_t read_all_tanks(void) {
    static uint32_t cycle_count = 0;
    cycle_count++;
    
    // Only reset every 20th cycle to reduce interference
    if (cycle_count % 20 == 0) {
        ESP_LOGD(TAG, "Resetting HX711 modules (cycle %lu)", cycle_count);
        reset_all_hx711_modules();
    }

    int successful_reads = 0;
    
    for (tank_id_t tank = 0; tank < TANK_MAX; tank++) {
        float weight = 0.0f;
        
        // Add safety checks
        __attribute__((unused)) volatile tank_config_t *config = &tank_configs[tank];
        
        // Try to read with error handling
        slave_pcb_err_t ret = read_tank_weight(tank, &weight);
        
        if (ret == SLAVE_PCB_OK) {
            successful_reads++;
            
            // Send weight data with minimal processing
            comm_msg_t msg = {
                .type = MSG_LOAD_CELL_DATA,
                .timestamp = esp_timer_get_time() / 1000,
                .data.load_cell_data = {
                    .tank = tank,
                    .weight = weight
                }
            };
            
            // Non-blocking send
            xQueueSend(loadcell_queue, &msg, 0);
            
            ESP_LOGD(TAG, "Tank %d: %.2f kg", tank, weight);
        } else {
            ESP_LOGD(TAG, "Tank %d read failed: %s", tank, get_error_string(ret));
        }
        
        // Longer delay for stability
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGD(TAG, "Read cycle completed: %d/%d tanks successful", successful_reads, TANK_MAX);
    return (successful_reads > 0) ? SLAVE_PCB_OK : SLAVE_PCB_ERR_TIMEOUT;
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
    
    // Reset filter for fresh calibration
    config->moving_avg_count = 0;
    config->moving_avg_index = 0;
    config->moving_avg_sum = 0.0f;
    config->stable_count = 0;

    // Take multiple readings for stable tare with outlier rejection
    uint32_t raw_values[HX711_CALIBRATION_SAMPLES / 2]; // Use fewer samples for tare
    int valid_readings = 0;
    
    ESP_LOGI(TAG, "Taking tare readings...");
    for (int i = 0; i < HX711_CALIBRATION_SAMPLES / 2; i++) {
        uint32_t raw_value;
        if (read_hx711_raw(config->dt_pin, &raw_value)) {
            raw_values[valid_readings++] = raw_value;
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Faster sampling
    }

    if (valid_readings < HX711_CALIBRATION_SAMPLES / 4) {
        ESP_LOGE(TAG, "Not enough valid readings for tare calibration (got %d)", valid_readings);
        return SLAVE_PCB_ERR_TIMEOUT;
    }

    // Remove outliers and calculate average
    uint32_t filtered_values[HX711_CALIBRATION_SAMPLES / 2];
    int filtered_count = remove_outliers(raw_values, valid_readings, filtered_values);
    
    uint32_t raw_sum = 0;
    for (int i = 0; i < filtered_count; i++) {
        raw_sum += filtered_values[i];
    }

    config->tare_offset = (float)(raw_sum / filtered_count);
    ESP_LOGI(TAG, "Tank %d tare calibration: samples=%d, filtered=%d, tare_offset=%.1f", 
             tank_id, valid_readings, filtered_count, config->tare_offset);

    return SLAVE_PCB_OK;
}

/**
 * @brief Get current system states
 */
uint32_t get_current_system_states(void) {
    return current_system_states;
}

/**
 * @brief Save calibration data to NVS
 */
static slave_pcb_err_t save_calibration_to_nvs(tank_id_t tank_id) {
    if (tank_id >= TANK_MAX) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return SLAVE_PCB_ERR_MEMORY;
    }

    // Create keys for this tank
    char tare_key[16], cal_key[16];
    snprintf(tare_key, sizeof(tare_key), "tare_%d", tank_id);
    snprintf(cal_key, sizeof(cal_key), "cal_%d", tank_id);

    // Save tare offset
    err = nvs_set_blob(nvs_handle, tare_key, &tank_configs[tank_id].tare_offset, sizeof(float));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save tare offset: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return SLAVE_PCB_ERR_MEMORY;
    }

    // Save calibration factor
    err = nvs_set_blob(nvs_handle, cal_key, &tank_configs[tank_id].calibration_factor, sizeof(float));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save calibration factor: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return SLAVE_PCB_ERR_MEMORY;
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return SLAVE_PCB_ERR_MEMORY;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Calibration data saved for tank %d", tank_id);
    return SLAVE_PCB_OK;
}

/**
 * @brief Load calibration data from NVS
 */
static slave_pcb_err_t load_calibration_from_nvs(tank_id_t tank_id) {
    if (tank_id >= TANK_MAX) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No calibration data found in NVS: %s", esp_err_to_name(err));
        return SLAVE_PCB_ERR_DEVICE_NOT_FOUND;
    }

    // Create keys for this tank
    char tare_key[16], cal_key[16];
    snprintf(tare_key, sizeof(tare_key), "tare_%d", tank_id);
    snprintf(cal_key, sizeof(cal_key), "cal_%d", tank_id);

    // Load tare offset
    size_t required_size = sizeof(float);
    err = nvs_get_blob(nvs_handle, tare_key, &tank_configs[tank_id].tare_offset, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load tare offset for tank %d: %s", tank_id, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return SLAVE_PCB_ERR_DEVICE_NOT_FOUND;
    }

    // Load calibration factor
    required_size = sizeof(float);
    err = nvs_get_blob(nvs_handle, cal_key, &tank_configs[tank_id].calibration_factor, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load calibration factor for tank %d: %s", tank_id, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return SLAVE_PCB_ERR_DEVICE_NOT_FOUND;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Calibration data loaded for tank %d: tare=%.1f, cal_factor=%.1f", 
             tank_id, tank_configs[tank_id].tare_offset, tank_configs[tank_id].calibration_factor);
    return SLAVE_PCB_OK;
}

/**
 * @brief Calibrate tank with known weight
 * @param tank_id Tank to calibrate
 * @param known_weight_kg Known weight in kg placed on the scale
 * @return slave_pcb_err_t Error code
 */
slave_pcb_err_t calibrate_tank_with_known_weight(tank_id_t tank_id, float known_weight_kg) {
    if (tank_id >= TANK_MAX) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    if (known_weight_kg <= 0.0f) {
        ESP_LOGE(TAG, "Known weight must be positive");
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting calibration for tank %d with known weight: %.2f kg", tank_id, known_weight_kg);
    
    tank_config_t *config = &tank_configs[tank_id];

    // Step 1: First do a tare calibration (with no weight)
    ESP_LOGI(TAG, "Step 1: Remove all weight and press enter to continue tare calibration...");
    
    // Wait a moment for user to remove weight
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    slave_pcb_err_t ret = calibrate_tank_tare(tank_id);
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to calibrate tare for tank %d", tank_id);
        return ret;
    }

    ESP_LOGI(TAG, "Step 2: Place the known weight (%.2f kg) on the scale...", known_weight_kg);
    ESP_LOGI(TAG, "Waiting 10 seconds for weight to stabilize...");
    
    // Wait for weight to stabilize
    vTaskDelay(pdMS_TO_TICKS(10000));

    // Step 2: Take readings with known weight
    uint32_t raw_sum = 0;
    int valid_readings = 0;

    ESP_LOGI(TAG, "Taking calibration readings...");
    
    // Use more samples for better calibration accuracy
    uint32_t raw_values[HX711_CALIBRATION_SAMPLES];
    valid_readings = 0;
    
    for (int i = 0; i < HX711_CALIBRATION_SAMPLES; i++) {
        uint32_t raw_value;
        if (read_hx711_raw(config->dt_pin, &raw_value)) {
            raw_values[valid_readings++] = raw_value;
            if (i % 10 == 0) {
                ESP_LOGI(TAG, "Progress: %d/%d readings taken", i+1, HX711_CALIBRATION_SAMPLES);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Faster sampling for calibration
    }

    if (valid_readings < HX711_CALIBRATION_SAMPLES * 3 / 4) {
        ESP_LOGE(TAG, "Not enough valid readings for calibration (got %d, need at least %d)", 
                 valid_readings, HX711_CALIBRATION_SAMPLES * 3 / 4);
        return SLAVE_PCB_ERR_TIMEOUT;
    }

    // Remove outliers and calculate average
    uint32_t filtered_values[HX711_CALIBRATION_SAMPLES];
    int filtered_count = remove_outliers(raw_values, valid_readings, filtered_values);
    
    raw_sum = 0;
    for (int i = 0; i < filtered_count; i++) {
        raw_sum += filtered_values[i];
    }

    uint32_t average_raw_with_weight = raw_sum / filtered_count;
    
    // Calculate calibration factor
    // calibration_factor = (raw_with_weight - tare_offset) / known_weight
    float raw_difference = (float)((int32_t)average_raw_with_weight - (int32_t)config->tare_offset);
    config->calibration_factor = raw_difference / known_weight_kg;

    ESP_LOGI(TAG, "Calibration completed for tank %d:", tank_id);
    ESP_LOGI(TAG, "  Samples taken: %d, filtered: %d, outliers removed: %d", 
             valid_readings, filtered_count, valid_readings - filtered_count);
    ESP_LOGI(TAG, "  Tare offset: %.1f", config->tare_offset);
    ESP_LOGI(TAG, "  Raw with weight: %lu", average_raw_with_weight);
    ESP_LOGI(TAG, "  Raw difference: %.1f", raw_difference);
    ESP_LOGI(TAG, "  Calibration factor: %.1f (raw units per kg)", config->calibration_factor);

    // Test the calibration by reading the current weight
    float test_weight;
    ret = read_tank_weight(tank_id, &test_weight);
    if (ret == SLAVE_PCB_OK) {
        ESP_LOGI(TAG, "Test reading: %.2f kg (should be close to %.2f kg)", test_weight, known_weight_kg);
        
        float error_percent = fabs(test_weight - known_weight_kg) / known_weight_kg * 100.0f;
        if (error_percent > 5.0f) {
            ESP_LOGW(TAG, "Calibration error is %.1f%% - may need recalibration", error_percent);
        } else {
            ESP_LOGI(TAG, "Calibration accuracy: %.1f%% error - Good!", error_percent);
        }
    }

    // Save calibration to NVS
    ret = save_calibration_to_nvs(tank_id);
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGW(TAG, "Failed to save calibration to NVS, but calibration is active for this session");
    }

    return SLAVE_PCB_OK;
}

/**
 * @brief Load all calibration data from NVS at startup
 */
static void load_all_calibrations_from_nvs(void) {
    ESP_LOGI(TAG, "Loading calibration data from NVS...");
    
    for (tank_id_t tank = 0; tank < TANK_MAX; tank++) {
        slave_pcb_err_t ret = load_calibration_from_nvs(tank);
        if (ret != SLAVE_PCB_OK) {
            ESP_LOGW(TAG, "No calibration data found for tank %d, using defaults", tank);
            // Keep default values (tare_offset = 0, calibration_factor = 1000.0)
        }
    }
}

/**
 * @brief Load Cell Manager initialization
 */
slave_pcb_err_t load_cell_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Load Cell Manager");

    // Initialize NVS for calibration data storage
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return SLAVE_PCB_ERR_MEMORY;
    }

    // Initialize HX711 SCK pin
    gpio_set_level(HX_711_SCK, 0);

    // Reset all HX711 modules for proper synchronization
    ESP_LOGI(TAG, "Resetting HX711 modules...");
    reset_all_hx711_modules();

    // Wait for HX711 modules to stabilize
    ESP_LOGI(TAG, "Waiting for HX711 modules to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(HX711_STABILIZING_TIME_MS));

    // Load saved calibration data from NVS
    load_all_calibrations_from_nvs();

    // DISABLED: Skip automatic tare calibration to prevent crashes
    // Only use saved calibration data or defaults
    ESP_LOGI(TAG, "Skipping automatic tare calibration for safety");
    ESP_LOGI(TAG, "Use manual calibration if needed: calibrate_tank_with_known_weight()");

    ESP_LOGI(TAG, "Load Cell Manager initialized successfully");
    return SLAVE_PCB_OK;
}

/**
 * @brief Load Cell Manager main task
 */
void load_cell_manager_task(void *pvParameters) {
    ESP_LOGI(TAG, "Load Cell Manager task started");

    const TickType_t reading_interval = pdMS_TO_TICKS(1000); // Faster reading: every 1 second
    TickType_t last_wake_time = xTaskGetTickCount();

    while (1) {
        uint32_t start_time = esp_timer_get_time() / 1000;
        
        // Read all tank weights sequentially
        slave_pcb_err_t ret = read_all_tanks();
        if (ret != SLAVE_PCB_OK) {
            ESP_LOGD(TAG, "Error reading tanks: %s", get_error_string(ret));
        }

        // Update system states based on weights
        update_system_states();

        uint32_t end_time = esp_timer_get_time() / 1000;
        uint32_t cycle_time = end_time - start_time;

        // Log current weights and performance periodically
        static uint32_t last_log_time = 0;
        uint32_t now = esp_timer_get_time() / 1000;
        
        if (now - last_log_time > 5000) { // Every 5 seconds instead of 10
            last_log_time = now;
            
            ESP_LOGI(TAG, "Weights (kg) - A:%.2f%s B:%.2f%s C:%.2f%s D:%.2f%s E:%.2f%s",
                     tank_configs[TANK_A].last_weight, tank_configs[TANK_A].is_stable ? "*" : "",
                     tank_configs[TANK_B].last_weight, tank_configs[TANK_B].is_stable ? "*" : "",
                     tank_configs[TANK_C].last_weight, tank_configs[TANK_C].is_stable ? "*" : "",
                     tank_configs[TANK_D].last_weight, tank_configs[TANK_D].is_stable ? "*" : "",
                     tank_configs[TANK_E].last_weight, tank_configs[TANK_E].is_stable ? "*" : "");
            
            ESP_LOGI(TAG, "Performance: cycle_time=%lums, states=0x%lx", cycle_time, current_system_states);
        }

        // Wait for next reading interval
        vTaskDelayUntil(&last_wake_time, reading_interval);
    }
}
