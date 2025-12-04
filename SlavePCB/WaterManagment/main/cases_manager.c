#include "cases_manager.h"


static const char *TAG = "CASE_MGR";

// Current system case for LED control
static volatile system_case_t current_case = CASE_RST;
static volatile uint32_t system_states = 0;
static volatile bool leds_transitioning = false;
static volatile bool hood_button_state = false;




/**
 * @brief Determine system case based on button clicks
 */
static system_case_t _determine_case_from_buttons(button_type_t button_id, click_type_t click_type) {
    click_type_t be1_click = CLICK_NONE;
    click_type_t be2_click = CLICK_NONE;
    click_type_t bd1_click = CLICK_NONE;
    click_type_t bd2_click = CLICK_NONE;
    click_type_t bv1_click = CLICK_NONE;
    click_type_t bv2_click = CLICK_NONE;
    click_type_t bp1_click = CLICK_NONE;
    click_type_t brst_click = CLICK_NONE;
    // Update the relevant click based on input
    switch (button_id) {
        case BUTTON_BE1:
            be1_click = click_type;
            break;
        case BUTTON_BE2:
            be2_click = click_type;
            break;
        case BUTTON_BD1:
            bd1_click = click_type;
            break;
        case BUTTON_BD2:
            bd2_click = click_type;
            break;
        case BUTTON_BV1:
            bv1_click = click_type;
            break;
        case BUTTON_BV2:
            bv2_click = click_type;
            break;
        case BUTTON_BP1:
            bp1_click = click_type;
            break;
        case BUTTON_BRST:
            brst_click = click_type;
            break;
        default:
            break;
    }
    // Priority: newest button click takes precedence
    
    // Check for reset requests first
    if (brst_click == CLICK_SHORT || be1_click == CLICK_LONG || bd1_click == CLICK_LONG) {
        return CASE_RST;
    }
    
    // Check for drainage/rain cases
    if (bv1_click == CLICK_SHORT) return CASE_V1;
    if (bv2_click == CLICK_SHORT) return CASE_V2;
    if (bp1_click == CLICK_PERMANENT) return CASE_P1;

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
 * @brief Check for BH button click to toggle hood state
 */
static void _check_button_bh_click(button_type_t button_id, click_type_t click_type) {
    if (button_id == BUTTON_BH) {
        if (click_type == CLICK_SHORT) {
            // Toggle hood state
            hood_button_state = !hood_button_state;
            

            if (hood_button_state) {
                esp_err_t ret = communications_send_command_with_ack(CMD_SET_HOOD_ON, NULL, 0, 1000);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to send set hood command");
                }
                else{
                    hood_state_t new_state = HOOD_ON;
                    update_hood_state(new_state);
                    ESP_LOGI(TAG, "Hood turned ON");
                    hood_button_state = true;
                    // Set BH LED accordingly
                    slave_pcb_err_t ret2 = button_bh_set_led(hood_button_state);
                    if (ret2 != SLAVE_PCB_OK) {
                        ESP_LOGE(TAG, "Failed to set BH LED state: %s", get_error_string(ret2));
                    }
                }

            } else {
                esp_err_t ret = communications_send_command_with_ack(CMD_SET_HOOD_OFF, NULL, 0, 1000);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to send set hood command");
                }
                else{
                    hood_state_t new_state = HOOD_OFF;
                    update_hood_state(new_state);
                    ESP_LOGI(TAG, "Hood turned OFF");
                    hood_button_state = false;
                    // Set BH LED accordingly
                    slave_pcb_err_t ret2 = button_bh_set_led(hood_button_state);
                    if (ret2 != SLAVE_PCB_OK) {
                        ESP_LOGE(TAG, "Failed to set BH LED state: %s", get_error_string(ret2));
                    }
                }
            }
        }
    }
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
 * @brief Apply case logic to all devices
 * @param case_id Case to apply
 * @return slave_pcb_err_t Error code
 */
slave_pcb_err_t apply_case_logic(system_case_t case_id) {
    if (case_id >= CASE_MAX) {
        ESP_LOGE(TAG, "Invalid case ID: %d", case_id);
        REPORT_ERROR(SLAVE_PCB_ERR_INVALID_ARG, TAG, "Invalid case ID", case_id);
        return SLAVE_PCB_ERR_INVALID_ARG;
    }

    // Check compatibility first
    if (!is_case_compatible(case_id, system_states)) {
        REPORT_ERROR(SLAVE_PCB_ERR_INCOMPATIBLE_CASE, TAG, 
                    "Case incompatible with current system state", system_states);
        return SLAVE_PCB_ERR_INCOMPATIBLE_CASE;
    }

    ESP_LOGI(TAG, "Applying case logic for %s", get_case_string(case_id));
    // Set LEDs to transitioning state
    set_leds_transitioning();
    // Delegate to electrovalve_pump_manager for the actual implementation
    slave_pcb_err_t ret = electrovalves_pumps_case_set(case_id);

    if (ret == SLAVE_PCB_OK) {
        // Update LEDs to reflect new case
        button_set_leds(case_id);
        ESP_LOGI(TAG, "Successfully applied case %s", get_case_string(case_id));
    } else {
        // Revert LEDs to previous case
        button_set_leds(current_case);
        ESP_LOGE(TAG, "Failed to apply case %s, error: %s", 
                 get_case_string(case_id), get_error_string(ret));
        REPORT_ERROR(ret, TAG, "Failed to apply case logic", 0);
                 
    }

    return ret;
    // return SLAVE_PCB_OK;
}

static void handle_button_event(button_type_t button_id, click_type_t click_type) {
    ESP_LOGI(TAG, "Button %d - Click %d", button_id, click_type);
    // Determine new case based on button inputs
    system_case_t new_case = _determine_case_from_buttons(button_id, click_type);
    if (new_case != current_case) {
        ESP_LOGI(TAG, "Transitioning to new case: %s", get_case_string(new_case));
        // Apply new case logic
        slave_pcb_err_t ret = apply_case_logic(new_case);
        if (ret == SLAVE_PCB_OK) {
            // Update current case
            current_case = new_case;
            // Update global state
            update_system_case(new_case);
        }

    }
    else{
        // Check for BH click to turn on/off the hood
        _check_button_bh_click(button_id, click_type);
    }
}

void cases_manager_task(void *pvParameters) {
    ESP_LOGI(TAG, "Cases Manager task started");

    static uint32_t last_pump_pe_active_time = 0;
    static uint32_t AUTO_RESET_TIMEOUT_MS = 600000; // 10 minutes

    while (1) {
        // Cas E1-E4 et D1-D4 : Surveillance de la pompe PE
        if (current_case == CASE_E1 || current_case == CASE_E2 || current_case == CASE_E3 || current_case == CASE_E4 ||
            current_case == CASE_D1 || current_case == CASE_D2 || current_case == CASE_D3 || current_case == CASE_D4) {
            
            if (pump_is_pumping(DEVICE_PUMP_PE)) {
                // La pompe fonctionne, reset le timer
                last_pump_pe_active_time = esp_timer_get_time() / 1000;
            } else {
                // Vérifier le timeout
                uint32_t inactive_time = (esp_timer_get_time() / 1000) - last_pump_pe_active_time;
                
                if (inactive_time > AUTO_RESET_TIMEOUT_MS) {
                    // Retour au cas RST
                    ESP_LOGI(TAG, "Auto-reset to CASE_RST after %d ms inactivity", inactive_time);
                    slave_pcb_err_t ret = apply_case_logic(CASE_RST);
                    if (ret == SLAVE_PCB_OK) {
                        current_case = CASE_RST;
                        // Update global state
                        update_system_case(current_case);
                    }
                }
            }
        } else {
            // Reset le timer si on n'est pas dans un cas utilisant PE
            last_pump_pe_active_time = esp_timer_get_time() / 1000;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


slave_pcb_err_t cases_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Cases Manager");
    
    // Initialize buttons manager and launch its task
    slave_pcb_err_t ret = buttons_manager_init();
    if (ret != SLAVE_PCB_OK) {
        REPORT_ERROR(SLAVE_PCB_ERR_INIT_FAIL, TAG, 
                    "Failed to initialize Button Manager", ret);
        return ret;
    }

    // Initialize electrovalves and pumps manager
    ret = electrovalves_pumps_init();
    if (ret != SLAVE_PCB_OK) {
        REPORT_ERROR(SLAVE_PCB_ERR_INIT_FAIL, TAG, 
                    "Failed to initialize Electrovalves and Pumps Manager", ret);
        return ret;
    }
    
    // Register button event handler
    register_click_callback(handle_button_event);
    xTaskCreate(buttons_manager_task, "button_manager", 4096, NULL, 5, NULL);

    // Create Cases Manager task
    xTaskCreate(cases_manager_task, "cases_manager", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Cases Manager initialized successfully");
    return SLAVE_PCB_OK;
}

