#include "electrovalve_executor.h"


static const char *TAG = "EV_EXECUTOR";

// Configuration centrale de toutes les électrovannes
static const electrovalve_config_t electrovalve_configs[] = {
    [DEVICE_ELECTROVALVE_A] = {DEVICE_ELECTROVALVE_A, "A", 0, true},
    [DEVICE_ELECTROVALVE_B] = {DEVICE_ELECTROVALVE_B, "B", 1, true},
    [DEVICE_ELECTROVALVE_C] = {DEVICE_ELECTROVALVE_C, "C", 2, true},
    [DEVICE_ELECTROVALVE_D] = {DEVICE_ELECTROVALVE_D, "D", 3, true},
    [DEVICE_ELECTROVALVE_E] = {DEVICE_ELECTROVALVE_E, "E", 4, true},
    [DEVICE_ELECTROVALVE_F] = {DEVICE_ELECTROVALVE_F, "F", 5, true},
};


static electrovalve_state_t electrovalve_states[DEVICE_MAX];

bool electrovalve_is_valid_device(device_type_t device) {
    return (device >= DEVICE_ELECTROVALVE_A && device <= DEVICE_ELECTROVALVE_F && 
            electrovalve_configs[device].device == device);
}

const electrovalve_config_t* electrovalve_get_config(device_type_t device) {
    if (!electrovalve_is_valid_device(device)) {
        return NULL;
    }
    return &electrovalve_configs[device];
}

slave_pcb_err_t electrovalve_init(void) {
    memset(electrovalve_states, 0, sizeof(electrovalve_states));
    
    // Compter le nombre d'électrovannes configurées
    int ev_count = 0;
    for (int i = DEVICE_ELECTROVALVE_A; i <= DEVICE_ELECTROVALVE_F; i++) {
        if (electrovalve_is_valid_device(i)) {
            ev_count++;
            ESP_LOGI(TAG, "Electrovalve %s configured on channel %d (sensor: %s)", 
                    electrovalve_configs[i].name, electrovalve_configs[i].i2c_channel,
                    electrovalve_configs[i].has_current_sensor ? "yes" : "no");
        }
    }
    
    ESP_LOGI(TAG, "Electrovalve executor initialized with %d electrovalves", ev_count);
    return SLAVE_PCB_OK;
}

slave_pcb_err_t electrovalve_set_state(device_type_t device, bool state) {
    if (!electrovalve_is_valid_device(device)) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    const electrovalve_config_t* config = electrovalve_get_config(device);
    if (!config) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    slave_pcb_err_t ret = set_output_state(device, state);
    
    if (ret == SLAVE_PCB_OK) {
        electrovalve_states[device].is_active = state;
        electrovalve_states[device].target_state = state;
        electrovalve_states[device].last_state_change = esp_timer_get_time() / 1000;
        
        ESP_LOGD(TAG, "Electrovalve %s set to %s", config->name, state ? "ON" : "OFF");
    }

    return ret;
}

bool electrovalve_is_turning(device_type_t device) {
    if (!electrovalve_is_valid_device(device)) {
        return false;
    }

    const electrovalve_config_t* config = electrovalve_get_config(device);
    if (!config || !config->has_current_sensor) {
        // Si pas de capteur, on suppose que l'électrovanne tourne si elle est active
        return electrovalve_states[device].is_active;
    }

    float current_ma;
    slave_pcb_err_t ret = current_sensor_read_channel(config->i2c_channel, &current_ma);
    
    if (ret == SLAVE_PCB_OK) {
        electrovalve_states[device].last_current_reading = current_ma;
        electrovalve_states[device].is_turning = (current_ma > CURRENT_THRESHOLD_EV_MA);
        
        if (electrovalve_states[device].is_active && !electrovalve_states[device].is_turning) {
            ESP_LOGI(TAG, "Electrovalve %s is active but not turning (current: %.1f mA) probably reached position", 
                    config->name, current_ma);
        }
        
        ESP_LOGD(TAG, "Electrovalve %s current: %.1f mA (%s)", 
                 config->name, current_ma, 
                 electrovalve_states[device].is_turning ? "TURNING" : "STOPPED");

        return electrovalve_states[device].is_turning;
    }

    return false;
}

