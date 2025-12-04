#include "buttons_manager.h"

static const char *TAG = "BTN_MGR";

// Button state tracking
static button_state_t button_states[BUTTON_MAX];

// Callback for button click events
static void (*button_click_callback)(button_type_t, click_type_t) = {0};

void register_click_callback(void (*callback)(button_type_t, click_type_t)) {
    button_click_callback = callback; // Register the callback function
}


/**
 * @brief Button Manager initialization
 */
slave_pcb_err_t buttons_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Button Manager");

    // Initialize button states
    memset(button_states, 0, sizeof(button_states));

    // Set initial LED state (all off)
    button_set_leds(CASE_RST);

    ESP_LOGI(TAG, "Button Manager initialized successfully");
    return SLAVE_PCB_OK;
}

/**
 * @brief Button Manager main task
 */
void buttons_manager_task(void *pvParameters) {
    ESP_LOGI(TAG, "Button Manager task started");

    static bool hood_state = false; // Track hood state (on/off)

    while (1) {

        // Check physical buttons (from BE1 to BH)
        for (button_type_t btn = BUTTON_BE1; btn <= BUTTON_BV2; btn++) {
            click_type_t click = detect_button_click(btn, &button_states[btn]);
            
            // Send button click event to registered callback
            if (click != CLICK_NONE) {
                button_click_callback(btn, click);
            }
        }

        // Lire les états virtuels (sans envoyer d'événements)
        bool bv1_state = button_states[BUTTON_BV1].virtual_state;
        bool bv2_state = button_states[BUTTON_BV2].virtual_state;
        bool bp1_state = button_states[BUTTON_BP1].virtual_state;
        bool brst_state = button_states[BUTTON_BRST].virtual_state;

        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz update rate
    }
}