#include "button_executor.h"


static const char *TAG = "BTN_EXEC";


static volatile bool leds_transitioning = false; // Track if LEDs are in transitioning state

// Mapping entre les boutons physiques et les commandes UART
static uart_button_cmd_t button_to_uart_cmd_map[] = {
    [BUTTON_BE1] = UART_CMD_BUTTON_E1,
    [BUTTON_BE2] = UART_CMD_BUTTON_E2,
    [BUTTON_BD1] = UART_CMD_BUTTON_D1,
    [BUTTON_BD2] = UART_CMD_BUTTON_D2,
    [BUTTON_BH] = UART_CMD_BUTTON_BH,
    [BUTTON_BV1] = UART_CMD_BUTTON_V1,
    [BUTTON_BV2] = UART_CMD_BUTTON_V2,
    [BUTTON_BP1] = UART_CMD_BUTTON_P1,
    [BUTTON_BRST] = UART_CMD_BUTTON_RST,
};

/**
 * @brief Read physical button state
 * @return true if pressed, false otherwise
 */
static bool _read_physical_button(button_type_t button) {
    int gpio_num = button_gpio_map[button];
    if (gpio_num < 0) {
        return false; // Virtual button
    }
    
    // Button is pressed when GPIO reads low (pull-up on pcb)
    return gpio_get_level(gpio_num) == 0;
}

/**
 * @brief Read UART simulated button state
 */
static bool _read_uart_button(button_type_t button) {
    if (button >= BUTTON_MAX) {
        return false;
    }
    
    uart_button_cmd_t uart_cmd = button_to_uart_cmd_map[button];
    return uart_manager_get_button_state(uart_cmd);
}

/**
 * @brief Read combined button state (UART has priority over physical)
 */
static bool _read_combined_button(button_type_t button, button_state_t *state) {
    // Priorité 1: Lecture UART (simulation)
    if (_read_uart_button(button)) {
        return true;
    }
    
    // Priorité 2: Bouton physique (si GPIO défini)
    int gpio_num = button_gpio_map[button];
    if (gpio_num >= 0) {
        return _read_physical_button(button);
    }
    
    // Priorité 3: État virtuel (pour les boutons sans GPIO)
    return state->virtual_state;
}

/**
 * @brief Detect button click type (short/long)
 */
click_type_t detect_button_click(button_type_t button, button_state_t *state) {
    if (button >= BUTTON_MAX) {
        return CLICK_NONE;
    }

    uint32_t now = esp_timer_get_time() / 1000;
    
    // Read current button state (physical or virtual)
    // bool current = (button_gpio_map[button] >= 0) ? 
    //                _read_physical_button(button) : state->virtual_state;
    
    // Read current button state (UART + physical + virtual)
    bool current = _read_combined_button(button, state);

    // Detect state change
    if (current != state->previous_state) {
        state->previous_state = current;
        state->last_change_time = now;
        
        if (current) {
            // Button pressed
            state->press_start_time = now;
        } else {
            // Button released
            uint32_t press_duration = now - state->press_start_time;
            
            if (press_duration > BUTTON_DEBOUNCE_MS) { // Debounce: minimum 50ms
                if (press_duration < BUTTON_LONG_CLICK_MS) {
                    state->last_click = CLICK_SHORT;
                    ESP_LOGI(TAG, "Button %s: Short click detected", get_button_string(button));
                    return CLICK_SHORT;
                } else {
                    state->last_click = CLICK_LONG;
                    ESP_LOGI(TAG, "Button %s: Long click detected", get_button_string(button));
                    return CLICK_LONG;
                }
            }
        }
    }

    state->current_state = current;
    return CLICK_NONE;
}

/**
 * @brief Set button LEDs according to case
 */
slave_pcb_err_t button_set_leds(system_case_t case_id) {
    if (case_id >= CASE_MAX) {
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Setting button LEDs for case %s", get_case_string(case_id));

    const led_colors_t *colors = &case_led_colors[case_id];
    
    slave_pcb_err_t ret = SLAVE_PCB_OK;
    
    // Set LED states using the generic set_output_state function
    ret |= set_output_state(DEVICE_LED_BE1_RED, colors->be1_red);
    ret |= set_output_state(DEVICE_LED_BE1_GREEN, colors->be1_green);
    ret |= set_output_state(DEVICE_LED_BE2_RED, colors->be2_red);
    ret |= set_output_state(DEVICE_LED_BE2_GREEN, colors->be2_green);
    ret |= set_output_state(DEVICE_LED_BD1_RED, colors->bd1_red);
    ret |= set_output_state(DEVICE_LED_BD1_GREEN, colors->bd1_green);
    ret |= set_output_state(DEVICE_LED_BD2_RED, colors->bd2_red);
    ret |= set_output_state(DEVICE_LED_BD2_GREEN, colors->bd2_green);

    if (ret == SLAVE_PCB_OK) {
        ESP_LOGI(TAG, "LEDs successfully set for case %s", get_case_string(case_id));
    } else {
        ESP_LOGE(TAG, "Failed to set LEDs for case %s", get_case_string(case_id));
        REPORT_ERROR(ret, TAG, "Failed to set LEDs for case", case_id);
    }

    return ret;
}

/**
 * @brief Set BH LED state
 */
slave_pcb_err_t button_bh_set_led(bool state) {
    ESP_LOGI(TAG, "Setting BH LED to state %d", state);
    return set_output_state(DEVICE_LED_BH, state);
}
/**
 * @brief Set LEDs to yellow (transitioning state)
 */
slave_pcb_err_t set_leds_transitioning(void) {
    ESP_LOGI(TAG, "Setting LEDs to transitioning state (yellow)");

    slave_pcb_err_t ret = SLAVE_PCB_OK;
    
    // All LEDs yellow (red + green) except BH
    ret |= set_output_state(DEVICE_LED_BE1_RED, true);
    ret |= set_output_state(DEVICE_LED_BE1_GREEN, true);
    ret |= set_output_state(DEVICE_LED_BE2_RED, true);
    ret |= set_output_state(DEVICE_LED_BE2_GREEN, true);
    ret |= set_output_state(DEVICE_LED_BD1_RED, true);
    ret |= set_output_state(DEVICE_LED_BD1_GREEN, true);
    ret |= set_output_state(DEVICE_LED_BD2_RED, true);
    ret |= set_output_state(DEVICE_LED_BD2_GREEN, true);

    leds_transitioning = true;
    return ret;
}