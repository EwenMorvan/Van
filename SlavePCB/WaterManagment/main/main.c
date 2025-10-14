#include "slave_pcb.h"
#include "wifi_ota_manager.h"
#include "ble_comm.h"

static const char *TAG = "SLAVE_PCB_MAIN";

// BLE communication
ble_comm_t ble_comm;
bool ble_connected = false;

// BLE configuration - Set this to true for ESP A (server), false for ESP B (client)
#define IS_ESP_A_SERVER false

// BLE callback functions
static void on_ble_connected(void) {
    ble_connected = true;
    ESP_LOGI(TAG, "BLE connected successfully");
}

static void on_ble_disconnected(void) {
    ble_connected = false;
    ESP_LOGI(TAG, "BLE disconnected");
}

static void on_ble_data_received(const char* data, size_t len) {
    ESP_LOGI(TAG, "BLE data received: %.*s", len, data);
    // TODO: Process received BLE data
}

// Global queues and mutexes
QueueHandle_t comm_queue = NULL;
QueueHandle_t case_queue = NULL;
QueueHandle_t button_queue = NULL;
QueueHandle_t loadcell_queue = NULL;
SemaphoreHandle_t output_mutex = NULL;

// Current system state
static volatile system_case_t current_case = CASE_RST;
static volatile uint32_t system_states = 0;

// Case compatibility matrix
static const uint32_t incompatible_cases[CASE_MAX] = {
    [CASE_RST] = 0,
    [CASE_E1] = STATE_CE | STATE_DF,
    [CASE_E2] = STATE_CE | STATE_RF,
    [CASE_E3] = STATE_DF | STATE_RE,
    [CASE_E4] = STATE_RF | STATE_RE,
    [CASE_D1] = STATE_CE | STATE_DF,
    [CASE_D2] = STATE_CE | STATE_RF,
    [CASE_D3] = STATE_DF | STATE_RE,
    [CASE_D4] = STATE_RF | STATE_RE,
    [CASE_V1] = STATE_DE,
    [CASE_V2] = STATE_RE,
    [CASE_P1] = STATE_RF
};

/**
 * @brief Generic function to set output state for any device
 * @param device Device type to control
 * @param state State to set (true/false, on/off)
 * @return slave_pcb_err_t Error code
 */
slave_pcb_err_t set_output_state(device_type_t device, bool state) {
    if (device >= DEVICE_MAX) {
        log_system_error(SLAVE_PCB_ERR_INVALID_ARG, "MAIN", "Invalid device type");
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Take mutex to ensure atomic operations
    if (xSemaphoreTake(output_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        log_system_error(SLAVE_PCB_ERR_TIMEOUT, "MAIN", "Failed to take output mutex");
        return SLAVE_PCB_ERR_TIMEOUT;
    }

    slave_pcb_err_t ret = SLAVE_PCB_OK;

    // ESP_LOGI(TAG, "Setting device %d to state %d", device, state);

    // Use shift register implementation for output control
    ret = shift_register_set_output_state(device, state);
    
    if (ret != SLAVE_PCB_OK) {
        log_system_error(ret, "MAIN", "Failed to set device output state");
    }

    xSemaphoreGive(output_mutex);
    return ret;
}

/**
 * @brief Check if a case is compatible with current system states
 * @param case_id Case to check
 * @param sys_states Current system states bitmask
 * @return true if compatible, false otherwise
 */
bool is_case_compatible(system_case_t case_id, uint32_t sys_states) {
    if (case_id >= CASE_MAX) {
        return false;
    }
    
    uint32_t incompatible = incompatible_cases[case_id];
    return (sys_states & incompatible) == 0;
}

/**
 * @brief Get error string from error code
 */
const char* get_error_string(slave_pcb_err_t error) {
    switch (error) {
        case SLAVE_PCB_OK: return "Success";
        case SLAVE_PCB_ERR_INVALID_ARG: return "Invalid argument";
        case SLAVE_PCB_ERR_DEVICE_NOT_FOUND: return "Device not found";
        case SLAVE_PCB_ERR_STATE_INVALID: return "Invalid state";
        case SLAVE_PCB_ERR_INCOMPATIBLE_CASE: return "Incompatible case";
        case SLAVE_PCB_ERR_TIMEOUT: return "Timeout";
        case SLAVE_PCB_ERR_I2C_FAIL: return "I2C failure";
        case SLAVE_PCB_ERR_SPI_FAIL: return "SPI failure";
        case SLAVE_PCB_ERR_MEMORY: return "Memory error";
        case SLAVE_PCB_ERR_COMM_FAIL: return "Communication failure";
        default: return "Unknown error";
    }
}

/**
 * @brief Get case string from case ID
 */
const char* get_case_string(system_case_t case_id) {
    switch (case_id) {
        case CASE_RST: return "RST";
        case CASE_E1: return "E1";
        case CASE_E2: return "E2";
        case CASE_E3: return "E3";
        case CASE_E4: return "E4";
        case CASE_D1: return "D1";
        case CASE_D2: return "D2";
        case CASE_D3: return "D3";
        case CASE_D4: return "D4";
        case CASE_V1: return "V1";
        case CASE_V2: return "V2";
        case CASE_P1: return "P1";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Apply case logic to all devices
 * @param case_id Case to apply
 * @return slave_pcb_err_t Error code
 */
static slave_pcb_err_t apply_case_logic(system_case_t case_id) {
    if (case_id >= CASE_MAX) {
        ESP_LOGE(TAG, "Invalid case ID: %d", case_id);
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Check compatibility first
    if (!is_case_compatible(case_id, system_states)) {
        ESP_LOGE(TAG, "Case %s is incompatible with current system states: 0x%lx", 
                 get_case_string(case_id), system_states);
        return SLAVE_PCB_ERR_INCOMPATIBLE_CASE;
    }

    ESP_LOGI(TAG, "Applying case logic for %s", get_case_string(case_id));

    // Delegate to electrovalve_pump_manager for the actual implementation
    slave_pcb_err_t ret = apply_electrovalve_pump_case(case_id);

    if (ret == SLAVE_PCB_OK) {
        current_case = case_id;
        ESP_LOGI(TAG, "Successfully applied case %s", get_case_string(case_id));
    } else {
        ESP_LOGE(TAG, "Failed to apply case %s, error: %s", 
                 get_case_string(case_id), get_error_string(ret));
    }

    return ret;
}

/**
 * @brief Initialize GPIO pins
 */
static slave_pcb_err_t init_gpio(void) {
    ESP_LOGI(TAG, "Initializing GPIO");

    // Configure input pins for buttons
    gpio_config_t input_config = {
        .pin_bit_mask = (1ULL << BE1) | (1ULL << BE2) | (1ULL << BD1) | 
                       (1ULL << BD2) | (1ULL << BH),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,// Already pulled down by PCB
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&input_config);
    if (ret != ESP_OK) {
        log_system_error(SLAVE_PCB_ERR_INVALID_ARG, "MAIN", "Failed to configure input GPIO");
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Configure output pins for shift registers and control signals
    gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << REG_MR) | (1ULL << REG_DS) | (1ULL << REG_STCP) |
                       (1ULL << REG_SHCP) | (1ULL << REG_OE) | (1ULL << HX_711_SCK) |
                       (1ULL << W5500_RST) | (1ULL << I2C_MUX_A0) | (1ULL << I2C_MUX_A1) |
                       (1ULL << I2C_MUX_A2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&output_config);
    if (ret != ESP_OK) {
        log_system_error(SLAVE_PCB_ERR_INVALID_ARG, "MAIN", "Failed to configure output GPIO");
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Configure HX711 DT pins as inputs
    gpio_config_t hx711_config = {
        .pin_bit_mask = (1ULL << HX_711_DT_A) | (1ULL << HX_711_DT_B) | 
                       (1ULL << HX_711_DT_C) | (1ULL << HX_711_DT_D) | 
                       (1ULL << HX_711_DT_E),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&hx711_config);
    if (ret != ESP_OK) {
        log_system_error(SLAVE_PCB_ERR_INVALID_ARG, "MAIN", "Failed to configure HX711 GPIO");
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Initialize shift registers
    slave_pcb_err_t shift_ret = init_shift_registers();
    if (shift_ret != SLAVE_PCB_OK) {
        log_system_error(shift_ret, "MAIN", "Failed to initialize shift registers");
        return shift_ret;
    }

    // Initialize HX711 clock
    gpio_set_level(HX_711_SCK, 0);
    
    // Initialize W5500 reset (active low, so set high for normal operation)
    gpio_set_level(W5500_RST, 1);
    
    // Initialize I2C multiplexer address pins
    gpio_set_level(I2C_MUX_A0, 0);
    gpio_set_level(I2C_MUX_A1, 0);
    gpio_set_level(I2C_MUX_A2, 0);

    ESP_LOGI(TAG, "GPIO initialization completed");
    return SLAVE_PCB_OK;
}

/**
 * @brief System monitoring task (simplified)
 */
static void system_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "System monitor task started (simplified mode)");

    while (1) {
        // Simple system health check without current monitoring
        // check_system_health();

        // Print status every 30 seconds
        static uint32_t last_status_print = 0;
        uint32_t now = esp_timer_get_time() / 1000000; // seconds
        
        if (now - last_status_print > 30) {
            last_status_print = now;
            ESP_LOGI(TAG, "System running - Current case: %s", get_case_string(current_case));
            print_shift_register_state();
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // Check every 5 seconds (less frequent)
    }
}

/**
 * @brief Main task that coordinates all managers
 */
static void main_coordinator_task(void *pvParameters) {
    comm_msg_t msg;
    
    ESP_LOGI(TAG, "Main coordinator task started");

    while (1) {
        // Check for messages from communication manager
        if (xQueueReceive(comm_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (msg.type) {
                case MSG_CASE_CHANGE:
                    ESP_LOGI(TAG, "Received case change request to %s", 
                             get_case_string(msg.data.case_data));
                    
                    slave_pcb_err_t result = apply_case_logic(msg.data.case_data);
                    if (result != SLAVE_PCB_OK) {
                        ESP_LOGE(TAG, "Failed to apply case %s: %s", 
                                 get_case_string(msg.data.case_data), get_error_string(result));
                        
                        // Send error message back
                        comm_msg_t error_msg = {
                            .type = MSG_ERROR,
                            .timestamp = esp_timer_get_time() / 1000,
                            .data.error_data = {
                                .error_code = result,
                            }
                        };
                        strcpy(error_msg.data.error_data.description, get_error_string(result));
                        xQueueSend(comm_queue, &error_msg, 0);
                    }
                    break;

                case MSG_RST_REQUEST:
                    ESP_LOGI(TAG, "Received RST request");
                    apply_case_logic(CASE_RST);
                    system_states = 0; // Clear all system states on reset
                    break;

                case MSG_ERROR:
                    ESP_LOGE(TAG, "Received error message: %s", 
                             msg.data.error_data.description);
                    break;

                default:
                    ESP_LOGW(TAG, "Unknown message type: %d", msg.type);
                    break;
            }
        }

        // Perform periodic system checks
        // TODO: Check system health, pump status, etc.
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


/**
 * @brief Calibration helper function - uncomment and modify as needed
 * This function can be called to calibrate specific tanks
 */
void perform_tank_calibration(void) {
    ESP_LOGI(TAG, "Starting tank calibration process...");
    ESP_LOGI(TAG, "Make sure to:");
    ESP_LOGI(TAG, "1. Remove all weight from the tank first");
    ESP_LOGI(TAG, "2. Wait for the system to stabilize");
    ESP_LOGI(TAG, "3. Place exactly the specified weight on the tank");
    
    // Wait for system to be ready
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // EXAMPLE CALIBRATIONS - Uncomment and modify as needed:
    
    // Calibrate Tank A with 8kg weight
    ESP_LOGI(TAG, "Calibrating Tank A with 4kg weight...");
    slave_pcb_err_t ret = calibrate_tank_with_known_weight(TANK_A, 4.0f);
    if (ret == SLAVE_PCB_OK) {
        ESP_LOGI(TAG, "Tank A calibration SUCCESS");
    } else {
        ESP_LOGE(TAG, "Tank A calibration FAILED: %s", get_error_string(ret));
    }
    
    // Calibrate Tank B with 10kg weight
    // ESP_LOGI(TAG, "Calibrating Tank B with 10kg weight...");
    // ret = calibrate_tank_with_known_weight(TANK_B, 10.0f);
    // if (ret == SLAVE_PCB_OK) {
    //     ESP_LOGI(TAG, "Tank B calibration SUCCESS");
    // } else {
    //     ESP_LOGE(TAG, "Tank B calibration FAILED: %s", get_error_string(ret));
    // }
    
    ESP_LOGI(TAG, "Calibration process completed. Uncomment specific calibrations as needed.");
}

// Calibration configuration - set to 0 to disable calibration on startup (debugging)
#define ENABLE_CALIBRATION_ON_STARTUP 0

void app_main(void) {
    ESP_LOGI(TAG, "SlavePCB starting up...");

    // Initialize GPIO
    slave_pcb_err_t ret = init_gpio();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to initialize GPIO: %s", get_error_string(ret));
        return;
    }

    // Create global mutex for output control
    output_mutex = xSemaphoreCreateMutex();
    if (output_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create output mutex");
        return;
    }

    // Create communication queues
    comm_queue = xQueueCreate(20, sizeof(comm_msg_t));
    case_queue = xQueueCreate(10, sizeof(system_case_t));
    button_queue = xQueueCreate(10, sizeof(comm_msg_t));
    loadcell_queue = xQueueCreate(10, sizeof(comm_msg_t));

    if (!comm_queue || !case_queue || !button_queue || !loadcell_queue) {
        ESP_LOGE(TAG, "Failed to create communication queues");
        return;
    }

    // Initialize only essential managers
    ret = communication_manager_init();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to initialize Communication Manager: %s", get_error_string(ret));
        return;
    }

    ret = electrovalve_pump_manager_init();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to initialize Electrovalve Pump Manager: %s", get_error_string(ret));
        return;
    }

    // ret = wifi_ota_init();
    // if (ret != SLAVE_PCB_OK) {
    //     ESP_LOGW(TAG, "Failed to initialize WiFi OTA: %s", get_error_string(ret));
    //     // Continue without WiFi/OTA - not critical for basic operation
    // }

    ret = button_manager_init();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to initialize Button Manager: %s", get_error_string(ret));
        return;
    }

    // // Initialize BLE communication
    // ret = ble_comm_init(&ble_comm, IS_ESP_A_SERVER);
    // if (ret == ESP_OK) {
    //     ble_comm_set_callbacks(on_ble_connected, on_ble_disconnected, on_ble_data_received);
    //     ret = ble_comm_start(&ble_comm);
    //     if (ret != ESP_OK) {
    //         ESP_LOGW(TAG, "Failed to start BLE Communication: %s", esp_err_to_name(ret));
    //     } else {
    //         ESP_LOGI(TAG, "BLE Communication initialized successfully");
    //     }
    // } else {
    //     ESP_LOGW(TAG, "Failed to initialize BLE Communication: %s", esp_err_to_name(ret));
    //     // Continue without BLE - not critical for basic operation
    // }

    // DISABLED: Load cell manager for now
    // ret = load_cell_manager_init();
    // if (ret != SLAVE_PCB_OK) {
    //     ESP_LOGE(TAG, "Failed to initialize Load Cell Manager: %s", get_error_string(ret));
    //     return;
    // }

    // Create only essential manager tasks
    // xTaskCreate(communication_manager_task, "comm_mgr", 4096, NULL, 5, NULL);
    xTaskCreate(electrovalve_pump_manager_task, "ev_pump_mgr", 4096, NULL, 4, NULL);
    xTaskCreate(button_manager_task, "btn_mgr", 4096, NULL, 3, NULL);
    // xTaskCreate(load_cell_manager_task, "loadcell_mgr", 3072, NULL, 3, NULL);
    
    // // Create main coordinator and system monitor tasks
    xTaskCreate(main_coordinator_task, "main_coord", 4096, NULL, 6, NULL);
    // xTaskCreate(system_monitor_task, "sys_monitor", 4096, NULL, 2, NULL);

    // Apply initial RST case
    apply_case_logic(CASE_RST);

    // Optional: Perform calibration on startup if enabled
#if ENABLE_CALIBRATION_ON_STARTUP
    ESP_LOGI(TAG, "Calibration enabled - starting calibration process in 5 seconds...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    perform_tank_calibration();
#endif

    ESP_LOGI(TAG, "SlavePCB initialization completed successfully");

}