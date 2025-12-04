#include "heater_manager.h"

static const char *TAG = "HEATER_MGR";
static adc_oneshot_unit_handle_t adc2_handle;

esp_err_t heater_manager_init(void){
    ESP_LOGI(TAG, "Initializing heater manager...");

    // Set diesel heater power sig pin as output
    gpio_config_t diesel_cfg = {
        .pin_bit_mask = (1ULL << HEATER_ON_SIG),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // Pas sur a verifier
        .intr_type = GPIO_INTR_DISABLE
    };

    // Set diesel heater Communication pin as bi-directional (handled by UART mux)
    gpio_config_t comms_cfg = {
        .pin_bit_mask = (1ULL << HEATER_TX),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    // Set the fuel level gauge pin as input
    gpio_config_t fuel_level_cfg = {
        .pin_bit_mask = (1ULL << FUEL_GAUGE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&diesel_cfg);
    gpio_config(&comms_cfg);
    gpio_config(&fuel_level_cfg);

    // Initialize ADC2 for fuel gauge
    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    esp_err_t ret = adc_oneshot_new_unit(&adc_config, &adc2_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC2");
        return ret;
    }
    adc_oneshot_chan_cfg_t adc_chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11
    };
    ret = adc_oneshot_config_channel(adc2_handle, FUEL_GAUGE_ADC_CHANNEL, &adc_chan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC2 channel");
        return ret;
    }

    // Intit the antifreeze liquide circulation pump
    ESP_LOGI(TAG, "Initializing pump manager...");
    ESP_ERROR_CHECK(pump_manager_init());
    // Init the radiator fan manager
    ESP_LOGI(TAG, "Initializing fan manager...");
    ESP_ERROR_CHECK(fan_manager_init());

    return ESP_OK;
}

uint8_t heater_manager_get_fuel_level(void){
    // The fuel gauge is a resisor 0 to 190 ohm in series with a 220 ohm resistor to GND
    // The voltage at the fuel gauge pin is then:
    // V_fuel = 3.3 * (R_fuel / (R_fuel + 220))
    int adc_reading = 0;
    esp_err_t ret = adc_oneshot_read(adc2_handle, FUEL_GAUGE_ADC_CHANNEL, &adc_reading);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read fuel level ADC");
        return 0;
    }
    // ESP_LOGI(TAG, "Fuel level ADC reading: %d", adc_reading);
    float voltage = (adc_reading / 4095.0) * 3.9; // Assuming 12-bit ADC with DB_11 attenuation (0-3.9V)
    // ESP_LOGI(TAG, "Fuel level voltage: %.2f V", voltage);

    // Calculate R_fuel from voltage
    float r_fuel = 0.0;
    float ratio = voltage / 3.3;
    float denom = 1.0 - ratio;
    if (ratio >= 1.0) {
        r_fuel = 190.0; // Max resistance when voltage >= 3.3V
    } else if (fabs(denom) < 0.001) {
        r_fuel = 190.0; // Max resistance when voltage ≈ 3.3V
    } else {
        r_fuel = (ratio * 220.0) / denom;
    }
    // ESP_LOGI(TAG, "Calculated fuel gauge resistance: %.2f ohms", r_fuel);
    // Calculate fuel level percentage
    uint8_t fuel_level_percent = (r_fuel / 190.0) * 100.0;

    return fuel_level_percent;
}

esp_err_t heater_manager_set_air_heater(bool state, uint8_t fan_speed_percent){
    ESP_LOGI(TAG, "Setting air heater to %s with fan speed %d%%", state ? "ON" : "OFF", fan_speed_percent);

    // Control the radiator fan speed
    esp_err_t err = fan_manager_set_speed(fan_speed_percent);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set fan speed: %s", esp_err_to_name(err));
        return err;
    }

    // Turn on the circulation pump
    err = pump_manager_set_state(state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set pump state: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t heater_manager_set_diesel_water_heater(bool state, uint8_t temperature){
    // TODO: Found how the heater communication protocol work to set target temperature and read status
    ESP_LOGI(TAG, "Setting diesel water heater to %s with target temperature %d°C", state ? "ON" : "OFF", temperature);

    // Control the diesel heater power signal
    gpio_set_level(HEATER_ON_SIG, state ? 1 : 0);

    // Here you would add code to send the target temperature to the diesel heater via UART mux

    return ESP_OK;
}

esp_err_t heater_manager_update_van_state(van_state_t* van_state){
    if(!van_state){
        return ESP_ERR_INVALID_ARG;
    }

    van_state->heater.heater_on = gpio_get_level(HEATER_ON_SIG) ? true : false; // TODO: Undersntant the protocol of the heater to get actual heater state
    van_state->heater.target_air_temperature = 22.0; // TODO: get actual target temperature
    van_state->heater.actual_air_temperature = 20.0; // TODO: get actual air temperature from sensor
    van_state->heater.antifreeze_temperature = 15.0; // TODO: get actual antifreeze temperature
    van_state->heater.fuel_level_percent = heater_manager_get_fuel_level();
    van_state->heater.error_code = 0; // TODO: get actual error code from heater
    van_state->heater.pump_active = pump_manager_get_state();
    van_state->heater.radiator_fan_speed = fan_manager_get_speed();
    return ESP_OK;
}