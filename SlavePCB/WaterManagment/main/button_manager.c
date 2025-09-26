#include "slave_pcb.h"
#include "ble_comm.h"

static const char *TAG = "BTN_MGR";


// Button state tracking
typedef struct {
    bool current_state;
    bool previous_state;
    uint32_t press_start_time;
    uint32_t last_change_time;
    click_type_t last_click;
    bool virtual_state; // For virtual buttons from MainPCB
} button_state_t;

static button_state_t button_states[BUTTON_MAX];

// Button to GPIO mapping
static const int button_gpio_map[BUTTON_MAX] = {
    [BUTTON_BE1] = BE1,
    [BUTTON_BE2] = BE2,
    [BUTTON_BD1] = BD1,
    [BUTTON_BD2] = BD2,
    [BUTTON_BH] = BH,
    [BUTTON_BV1] = -1,  // Virtual button
    [BUTTON_BV2] = -1,  // Virtual button
    [BUTTON_BP1] = -1,  // Virtual button
    [BUTTON_BRST] = -1, // Virtual button
};

// LED color definitions for each case
typedef struct {
    bool be1_red;
    bool be1_green;
    bool be2_red;
    bool be2_green;
    bool bd1_red;
    bool bd1_green;
    bool bd2_red;
    bool bd2_green;
} led_colors_t;

static const led_colors_t case_led_colors[CASE_MAX] = {
    [CASE_RST] = {0, 0, 0, 0, 0, 0, 0, 0}, // All off
    [CASE_E1]  = {0, 1, 1, 0, 0, 0, 0, 0}, // BE1 Green, BE2 Red
    [CASE_E2]  = {0, 1, 0, 1, 0, 0, 0, 0}, // BE1 Green, BE2 Green
    [CASE_E3]  = {1, 0, 1, 0, 0, 0, 0, 0}, // BE1 Red, BE2 Red
    [CASE_E4]  = {1, 0, 0, 1, 0, 0, 0, 0}, // BE1 Red, BE2 Green
    [CASE_D1]  = {0, 0, 0, 0, 0, 1, 1, 0}, // BD1 Green, BD2 Red
    [CASE_D2]  = {0, 0, 0, 0, 0, 1, 0, 1}, // BD1 Green, BD2 Green
    [CASE_D3]  = {0, 0, 0, 0, 1, 0, 1, 0}, // BD1 Red, BD2 Red
    [CASE_D4]  = {0, 0, 0, 0, 1, 0, 0, 1}, // BD1 Red, BD2 Green
    [CASE_V1]  = {0, 0, 0, 0, 0, 0, 0, 0}, // All off
    [CASE_V2]  = {0, 0, 0, 0, 0, 0, 0, 0}, // All off
    [CASE_P1]  = {0, 0, 0, 0, 0, 0, 0, 0}, // All off
};

// Current system case for LED control
static volatile system_case_t current_case = CASE_RST;
static volatile bool leds_transitioning = false;

// External reference to BLE communication structure
extern ble_comm_t ble_comm;
extern bool ble_connected;

/**
 * @brief Send BLE message for hood control
 */
static void send_ble_hood_message(bool hood_state) {
    if (!ble_connected) {
        ESP_LOGW(TAG, "BLE not connected, cannot send hood message");
        return;
    }

    // Create very short message to test MTU limits
    char ble_message[64];
    snprintf(ble_message, sizeof(ble_message),
             "{\"type\":\"command\",\"cmd\":\"set_hood_state\",\"target\":0,\"value\":%d}", hood_state ? 1 : 0);

    size_t msg_len = strlen(ble_message);
    ESP_LOGI(TAG, "Sending BLE message (len=%d): %s", msg_len, ble_message);

    esp_err_t ret = ble_comm_send(&ble_comm, ble_message, msg_len);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "BLE hood command sent successfully");
    } else {
        ESP_LOGW(TAG, "Failed to send BLE hood command: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief Read physical button state
 */
static bool read_physical_button(button_type_t button) {
    int gpio_num = button_gpio_map[button];
    if (gpio_num < 0) {
        return false; // Virtual button
    }
    
    // Button is pressed when GPIO reads low (pull-up on pcb)
    return gpio_get_level(gpio_num) == 0;
}

/**
 * @brief Detect button click type (short/long)
 */
static click_type_t detect_button_click(button_type_t button) {
    if (button >= BUTTON_MAX) {
        return CLICK_NONE;
    }

    button_state_t *state = &button_states[button];
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Read current button state (physical or virtual)
    bool current = (button_gpio_map[button] >= 0) ? 
                   read_physical_button(button) : state->virtual_state;

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
            
            if (press_duration > CONFIG_SLAVEPCB_BUTTON_DEBOUNCE_MS) { // Debounce: minimum 50ms
                if (press_duration < CONFIG_SLAVEPCB_LONG_CLICK_MS) {
                    state->last_click = CLICK_SHORT;
                    ESP_LOGI(TAG, "Button %d: Short click detected", button);
                    return CLICK_SHORT;
                } else {
                    state->last_click = CLICK_LONG;
                    ESP_LOGI(TAG, "Button %d: Long click detected", button);
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
static slave_pcb_err_t set_button_leds(system_case_t case_id) {
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
    }

    return ret;
}

/**
 * @brief Set LEDs to yellow (transitioning state)
 */
static slave_pcb_err_t set_leds_transitioning(void) {
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

/**
 * @brief Determine system case based on button clicks
 */
static system_case_t determine_case_from_buttons(click_type_t be1_click, click_type_t be2_click,
                                                click_type_t bd1_click, click_type_t bd2_click,
                                                bool bv1_state, bool bv2_state, bool bp1_state, bool brst_state) {
    
    // Priority: newest button click takes precedence
    
    // Check for reset requests first
    if (brst_state || be1_click == CLICK_LONG || bd1_click == CLICK_LONG) {
        return CASE_RST;
    }
    
    // Check for drainage/rain cases
    if (bv1_state) return CASE_V1;
    if (bv2_state) return CASE_V2;
    if (bp1_state) return CASE_P1;
    
    // Check for kitchen cases
    if (be1_click == CLICK_SHORT || be2_click == CLICK_SHORT) {
        // Determine current kitchen state
        if (current_case >= CASE_E1 && current_case <= CASE_E4) {
            // Already in kitchen mode, toggle states
            if (be1_click == CLICK_SHORT) {
                // Toggle between clean (E1/E2) and recycled water (E3/E4)
                switch (current_case) {
                    case CASE_E1: return CASE_E3;
                    case CASE_E2: return CASE_E4;
                    case CASE_E3: return CASE_E1;
                    case CASE_E4: return CASE_E2;
                    default: return CASE_E1;
                }
            } else if (be2_click == CLICK_SHORT) {
                // Toggle recycling state
                switch (current_case) {
                    case CASE_E1: return CASE_E2;
                    case CASE_E2: return CASE_E1;
                    case CASE_E3: return CASE_E4;
                    case CASE_E4: return CASE_E3;
                    default: return CASE_E1;
                }
            }
        } else {
            // Start with clean water case
            return CASE_E1;
        }
    }
    
    // Check for shower cases
    if (bd1_click == CLICK_SHORT || bd2_click == CLICK_SHORT) {
        // Similar logic for shower
        if (current_case >= CASE_D1 && current_case <= CASE_D4) {
            if (bd1_click == CLICK_SHORT) {
                // Toggle between clean and recycled water
                switch (current_case) {
                    case CASE_D1: return CASE_D3;
                    case CASE_D2: return CASE_D4;
                    case CASE_D3: return CASE_D1;
                    case CASE_D4: return CASE_D2;
                    default: return CASE_D1;
                }
            } else if (bd2_click == CLICK_SHORT) {
                // Toggle recycling state
                switch (current_case) {
                    case CASE_D1: return CASE_D2;
                    case CASE_D2: return CASE_D1;
                    case CASE_D3: return CASE_D4;
                    case CASE_D4: return CASE_D3;
                    default: return CASE_D1;
                }
            }
        } else {
            return CASE_D1;
        }
    }
    
    // No change
    return current_case;
}

/**
 * @brief Button Manager initialization
 */
slave_pcb_err_t button_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Button Manager");

    // Initialize button states
    memset(button_states, 0, sizeof(button_states));

    // Set initial LED state (all off)
    set_button_leds(CASE_RST);

    ESP_LOGI(TAG, "Button Manager initialized successfully");
    return SLAVE_PCB_OK;
}

/**
 * @brief Button Manager main task
 */
void button_manager_task(void *pvParameters) {
    ESP_LOGI(TAG, "Button Manager task started");

    static bool hood_state = false; // Track hood state (on/off)

    while (1) {
        // Detect button clicks
        click_type_t be1_click = detect_button_click(BUTTON_BE1);
        click_type_t be2_click = detect_button_click(BUTTON_BE2);
        click_type_t bd1_click = detect_button_click(BUTTON_BD1);
        click_type_t bd2_click = detect_button_click(BUTTON_BD2);
        click_type_t bh_click = detect_button_click(BUTTON_BH); // BH button handling

        // Get virtual button states
        bool bv1_state = button_states[BUTTON_BV1].virtual_state;
        bool bv2_state = button_states[BUTTON_BV2].virtual_state;
        bool bp1_state = button_states[BUTTON_BP1].virtual_state;
        bool brst_state = button_states[BUTTON_BRST].virtual_state;

        // Determine new case based on button inputs
        system_case_t new_case = determine_case_from_buttons(
            be1_click, be2_click, bd1_click, bd2_click,
            bv1_state, bv2_state, bp1_state, brst_state);

        // Check for hood button (BH)
        if (bh_click == CLICK_SHORT || bh_click == CLICK_LONG) {
            hood_state = !hood_state; // Swap hood state (on/off)

            // Display "Hood On" or "Hood Off"
            if (hood_state) {
                ESP_LOGI(TAG, "Hood Button clicked - Hood On requested");
            } else {
                ESP_LOGI(TAG, "Hood Button clicked - Hood Off requested");
            }

            // Update the state of the LED associated with the BH button
            set_output_state(DEVICE_LED_BH, hood_state);

            // Send BLE message for hood control
            send_ble_hood_message(hood_state);
        }

        // If case changed, send message to main coordinator
        if (new_case != current_case) {
            ESP_LOGI(TAG, "Case change requested: %s -> %s", 
                     get_case_string(current_case), get_case_string(new_case));

            // Set LEDs to transitioning state briefly
            set_leds_transitioning();

            comm_msg_t msg = {
                .type = MSG_CASE_CHANGE,
                .timestamp = esp_timer_get_time() / 1000,
                .data.case_data = new_case
            };

            if (xQueueSend(comm_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Failed to send case change message");
                // If message sending fails, revert LEDs immediately
                ESP_LOGW(TAG, "Reverting LEDs due to message send failure");
                leds_transitioning = false;
                set_button_leds(current_case);
            } else {
                ESP_LOGI(TAG, "Case change message sent successfully");
                
                // Update case immediately - LEDs will be updated by notify_transition_complete()
                current_case = new_case;
                ESP_LOGI(TAG, "Case updated to: %s, waiting for transition complete notification", get_case_string(current_case));
            }

            // Clear virtual button states after processing
            button_states[BUTTON_BV1].virtual_state = false;
            button_states[BUTTON_BV2].virtual_state = false;
            button_states[BUTTON_BP1].virtual_state = false;
            button_states[BUTTON_BRST].virtual_state = false;

            // Send button state to communication manager
            comm_msg_t btn_msg = {
                .type = MSG_BUTTON_STATE,
                .timestamp = esp_timer_get_time() / 1000,
                .data.button_data = {
                    .button = BUTTON_BE1, // This could be more specific
                    .state = true
                }
            };
            xQueueSend(button_queue, &btn_msg, 0);
        }

        // Simple delay without complex timeout management
        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz update rate
    }
}

/**
 * @brief Notify button manager that case transition is complete
 * This should be called by electrovalve_pump_manager when case change is finished
 */
void button_manager_notify_transition_complete(void) {
    ESP_LOGI(TAG, "Transition complete notification received");
    
    if (leds_transitioning) {
        ESP_LOGI(TAG, "Clearing transitioning state and setting LEDs for current case: %s", get_case_string(current_case));
        leds_transitioning = false;
        set_button_leds(current_case);
        ESP_LOGI(TAG, "Transition complete - LEDs updated successfully");
    } else {
        ESP_LOGI(TAG, "LEDs were not in transitioning state, no update needed");
    }
}


