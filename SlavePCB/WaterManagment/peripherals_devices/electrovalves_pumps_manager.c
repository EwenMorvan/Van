#include "electrovalves_pumps_manager.h"



static const char *TAG = "EV_PUMP_MGR";

static slave_pcb_err_t wait_for_electrovalves_ready(void) {
    ESP_LOGI(TAG, "Waiting for electrovalves to reach position...");
    
    const uint32_t timeout_ms = 20000; // 20 secondes timeout
    const uint32_t start_time = esp_timer_get_time() / 1000;
    
    while (electrovalve_is_turning(DEVICE_ELECTROVALVE_A) ||
           electrovalve_is_turning(DEVICE_ELECTROVALVE_B) ||
           electrovalve_is_turning(DEVICE_ELECTROVALVE_C) ||
           electrovalve_is_turning(DEVICE_ELECTROVALVE_D) ||
           electrovalve_is_turning(DEVICE_ELECTROVALVE_E) ||
           electrovalve_is_turning(DEVICE_ELECTROVALVE_F)) {
        
        // Vérifier le timeout
        uint32_t current_time = esp_timer_get_time() / 1000;
        if (current_time - start_time > timeout_ms) {
            ESP_LOGW(TAG, "Timeout after 20s waiting for electrovalves to reach position");
            return SLAVE_PCB_ERR_TIMEOUT;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Electrovalves reached position");
    return SLAVE_PCB_OK;
}

slave_pcb_err_t electrovalves_pumps_init(void) {
    ESP_LOGI(TAG, "Initializing Electrovalves and Pumps Manager");

    // Initialize all subsystems
    slave_pcb_err_t ret = i2c_manager_init();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "I2C manager init failed");
        REPORT_ERROR(ret, TAG, "Failed to initialize I2C manager", 0);
        return ret;
    }

    ret = current_sensor_init();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Current sensor init failed");
        REPORT_ERROR(ret, TAG, "Failed to initialize current sensor", 0);
        return ret;
    }

    ret = electrovalve_init();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Electrovalve executor init failed");
        REPORT_ERROR(ret, TAG, "Failed to initialize electrovalve executor", 0);
        return ret;
    }

    ret = pump_init();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Pump executor init failed");
        REPORT_ERROR(ret, TAG, "Failed to initialize pump executor", 0);
        return ret;
    }

    // Initialize multiplexer GPIOs
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << I2C_MUX_A0) | (1ULL << I2C_MUX_A1) | (1ULL << I2C_MUX_A2),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    // Set initial state
    gpio_set_level(I2C_MUX_A0, 0);
    gpio_set_level(I2C_MUX_A1, 0);
    gpio_set_level(I2C_MUX_A2, 0);

    ESP_LOGI(TAG, "Electrovalves and Pumps Manager initialized successfully");
    return SLAVE_PCB_OK;
}

slave_pcb_err_t electrovalves_pumps_case_set(system_case_t case_id) {
    ESP_LOGI(TAG, "Applying case %s", get_case_string(case_id));

    if (case_id >= CASE_MAX) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Case logic table (simplified)
    typedef struct {
        bool ev_a, ev_b, ev_c, ev_d, ev_e;
        bool pump_pe, pump_pd, pump_pv, pump_pp;
    } case_logic_t;

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

    // Before eatch transition to a case, we ensure that all pumps are off
    ret |= pump_set_state(DEVICE_PUMP_PE, false);
    ret |= pump_set_state(DEVICE_PUMP_PD, false);
    ret |= pump_set_state(DEVICE_PUMP_PV, false);
    ret |= pump_set_state(DEVICE_PUMP_PP, false);

    // Set electrovalves
    ret |= electrovalve_set_state(DEVICE_ELECTROVALVE_A, logic->ev_a);
    ret |= electrovalve_set_state(DEVICE_ELECTROVALVE_B, logic->ev_b);
    ret |= electrovalve_set_state(DEVICE_ELECTROVALVE_C, logic->ev_c);
    ret |= electrovalve_set_state(DEVICE_ELECTROVALVE_D, logic->ev_d);
    ret |= electrovalve_set_state(DEVICE_ELECTROVALVE_E, logic->ev_e);

    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to set electrovalves");
        REPORT_ERROR(ret, TAG, "Failed to set electrovalves", 0);
        return ret;
    }

    // Wait for electrovalves
    ret = wait_for_electrovalves_ready();
    if (ret != SLAVE_PCB_OK) return ret;

    // Set pumps
    ret |= pump_set_state(DEVICE_PUMP_PE, logic->pump_pe);
    ret |= pump_set_state(DEVICE_PUMP_PD, logic->pump_pd);
    ret |= pump_set_state(DEVICE_PUMP_PV, logic->pump_pv);
    ret |= pump_set_state(DEVICE_PUMP_PP, logic->pump_pp);

    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to set pumps");
        REPORT_ERROR(ret, TAG, "Failed to set pumps", 0);
        return ret;
    }
    ESP_LOGI(TAG, "Case %s applied successfully", get_case_string(case_id));
    return SLAVE_PCB_OK;
}
