#include "pump_executor.h"


static const char *TAG = "PUMP_EXECUTOR";

// Configuration centrale de toutes les pompes
static const pump_config_t pump_configs[] = {
    [DEVICE_PUMP_PE] = {DEVICE_PUMP_PE, "PE", 6, true, CURRENT_THRESHOLD_PUMP_PE_MA},
    [DEVICE_PUMP_PD] = {DEVICE_PUMP_PD, "PD", -1, false, 0},
    [DEVICE_PUMP_PV] = {DEVICE_PUMP_PV, "PV", 7, true, CURRENT_THRESHOLD_PUMP_PV_EMPTY_MA},
    [DEVICE_PUMP_PP] = {DEVICE_PUMP_PP, "PP", -1, false, 0},
};


static pump_state_t pump_states[DEVICE_MAX];

bool pump_is_valid_device(device_type_t device) {
    return (device >= DEVICE_PUMP_PE && device <= DEVICE_PUMP_PP && 
            pump_configs[device].device == device);
}

const pump_config_t* pump_get_config(device_type_t device) {
    if (!pump_is_valid_device(device)) {
        return NULL;
    }
    return &pump_configs[device];
}

slave_pcb_err_t pump_init(void) {
    memset(pump_states, 0, sizeof(pump_states));
    
    // Compter le nombre de pompes configurées
    int pump_count = 0;
    for (int i = DEVICE_PUMP_PE; i <= DEVICE_PUMP_PP; i++) {
        if (pump_is_valid_device(i)) {
            pump_count++;
            ESP_LOGI(TAG, "Pump %s configured on channel %d (sensor: %s)", 
                    pump_configs[i].name, pump_configs[i].i2c_channel,
                    pump_configs[i].has_current_sensor ? "yes" : "no");
        }
    }
    
    ESP_LOGI(TAG, "Pump executor initialized with %d pumps", pump_count);
    return SLAVE_PCB_OK;
}

slave_pcb_err_t pump_set_state(device_type_t device, bool state) {
    if (!pump_is_valid_device(device)) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    const pump_config_t* config = pump_get_config(device);
    if (!config) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    slave_pcb_err_t ret = set_output_state(device, state);
    
    if (ret == SLAVE_PCB_OK) {
        pump_states[device].is_active = state;
        pump_states[device].target_state = state;
        pump_states[device].last_state_change = esp_timer_get_time() / 1000;
        
        ESP_LOGD(TAG, "Pump %s set to %s", config->name, state ? "ON" : "OFF");
    }

    return ret;
}

bool pump_is_pumping(device_type_t device) {
    if (!pump_is_valid_device(device)) {
        return false;
    }

    const pump_config_t* config = pump_get_config(device);
    if (!config || !config->has_current_sensor) {
        // Si pas de capteur, on suppose que la pompe fonctionne si elle est active
        return pump_states[device].is_active;
    }

    float current_ma;
    slave_pcb_err_t ret = current_sensor_read_channel(config->i2c_channel, &current_ma);
    
    if (ret == SLAVE_PCB_OK) {
        pump_states[device].last_current_reading = current_ma;
        pump_states[device].is_pumping = (current_ma > config->pumping_current_threshold_ma);
        
        if (pump_states[device].is_active && !pump_states[device].is_pumping) {
            ESP_LOGI(TAG, "Pump %s is active but not pumping water (current: %.1f mA)", 
                    config->name, current_ma);
        }
        ESP_LOGD(TAG, "Pump %s current: %.1f mA - Pumping: %s", 
                config->name, current_ma, 
                pump_states[device].is_pumping ? "YES" : "NO");
        
        return pump_states[device].is_pumping;
    }

    return false;
}

float pump_get_current(device_type_t device) {
    if (!pump_is_valid_device(device)) {
        return 0.0f;
    }
    
    return pump_states[device].last_current_reading;
}