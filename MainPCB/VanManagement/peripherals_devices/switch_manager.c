/**
 * @brief Switch manager module is responsible for handling the input from the physical switches
 * It debounces switch inputs, detects short and long presses, it send number of clicks and
 * a progressive value (0-100) during long press, to the global_coordinator.c
 */
#include "switch_manager.h"

static const char *TAG = "SWITCH_MGR";

static void handle_switch_input(void);
static void handle_door_input(void);

/**
 * @brief Read combined button state (UART has priority over physical)
 */
static bool _read_combined_button() {
    bool state_uart = uart_com_reader();
    int state_gpio = gpio_get_level(INTER);
    // Priorité 1: Lecture UART (simulation)
    if (state_uart) {
        return true;
    }
    
    // Priorité 2: Bouton physique (si GPIO défini)
    if (state_gpio == 1) {
        return true;
    }
    
    return false;
}

esp_err_t switch_manager_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initialisation of the switch manager...");

    // Configure switch input pin
    gpio_config_t switch_config = {
        .pin_bit_mask = (1ULL << INTER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE};

    ret = gpio_config(&switch_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure switch input pin");
        return ret;
    }

    // Configure door read input pin
    gpio_config_t door_switch_config = {
        .pin_bit_mask = (1ULL << VAN_LIGHT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    ret = gpio_config(&door_switch_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure door switch input pin");
        return ret;
    }

    // Create switch task
    BaseType_t result = xTaskCreate(
        switch_manager_task,
        "switch_task",
        4096,
        NULL,
        5,
        NULL);

    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create switch task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Switch manager initialized");
    return ESP_OK;
}

void switch_manager_task(void *parameters)
{
    ESP_LOGI(TAG, "Switch manager task started");

    while (1)
    {
        handle_switch_input();
        handle_door_input();
        vTaskDelay(pdMS_TO_TICKS(50)); // 100Hz update rate
    }
}

bool get_door_state(void)
{
    return (gpio_get_level(VAN_LIGHT) == 0); // LOW means door open
}

static void handle_door_input(void)
{
    static bool prev_door_state = false;

    bool door_state = get_door_state();

    if (door_state != prev_door_state) {
        prev_door_state = door_state;
        if (door_state) {
            ESP_LOGI(TAG, "Door opened or van unlocked");
            global_coordinator_publish(GC_EVT_DOOR_VALUE_CHANGED, 1);
        } else {
            ESP_LOGI(TAG, "Van automatic light timeout reached");
            global_coordinator_publish(GC_EVT_DOOR_VALUE_CHANGED, 0);
        }
    }
}

static void handle_switch_input(void)
{
    static bool prev_state = false;
    static uint32_t press_start_time = 0;
    static uint32_t last_release_time = 0;
    static uint32_t last_debounce_time = 0;
    static int click_count = 0;
    static bool long_press_active = false;
    static int value = 0;   // valeur de sortie 0–100
    static bool short_click_pending = false;

    bool state = _read_combined_button();
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // ---- Debounce ----
    uint32_t debounce_elapsed = now - last_debounce_time;
    if (debounce_elapsed < SWITCH_DEBOUNCE_MS) {
        return;
    }

    // ---- Détection du changement d'état ----
    if (state != prev_state) {
        last_debounce_time = now;
        ESP_LOGD(TAG, "State changed: %s after %u ms", state ? "PRESSED" : "RELEASED", debounce_elapsed);

        if (state) {
            // Bouton pressé
            press_start_time = now;
            long_press_active = false;
        } else {
            // Bouton relâché
            uint32_t press_duration = now - press_start_time;

            if (press_duration < SWITCH_SHORT_PRESS_MS) {
                // ---- Short press détecté ----
                uint32_t time_since_last_click = now - last_release_time;
                
                // Fenêtre de temps plus large pour les clics multiples
                if (time_since_last_click < SWITCH_MULTI_CLICK_MS ) {
                    click_count++;
                    ESP_LOGI(TAG, "Multi-click detected: count=%d (time since last: %u ms)", 
                             click_count, time_since_last_click);
                } else {
                    click_count = 1;
                    ESP_LOGI(TAG, "New click sequence started");
                }

                last_release_time = now;
                short_click_pending = true; // On marque un short click en attente
            } else if (long_press_active) {
                // ---- Fin du long press ----
                ESP_LOGI(TAG, "Long press ended, final value=%d", value);
                global_coordinator_publish(GC_EVT_SWITCH_LONG_PRESS_FINAL, value);
            }
        }

        prev_state = state;
    }

    // ---- Gestion du long press progressif ----
    if (state) {
        uint32_t press_duration = now - press_start_time;

        if (press_duration > SWITCH_LONG_PRESS_MS) {
            long_press_active = true;

            const int MIN_VAL = 10;
            const int MAX_VAL = 255;
            uint32_t cycle = SWITCH_LONG_CYCLE_MS;
            uint32_t half = (cycle / 2) ? (cycle / 2) : 1;
            uint32_t phase = (press_duration - SWITCH_LONG_PRESS_MS) % cycle;

            if (phase < half) {
            float ratio = (float)phase / (float)half; // 0..1
            value = (int)(MIN_VAL + ratio * (MAX_VAL - MIN_VAL));
            } else {
            float ratio = (float)(phase - half) / (float)half; // 0..1
            value = (int)(MAX_VAL - ratio * (MAX_VAL - MIN_VAL));
            }

            if (value < MIN_VAL) value = MIN_VAL;
            if (value > MAX_VAL) value = MAX_VAL;

            ESP_LOGI(TAG, "Long press value: %d", value);
            global_coordinator_publish(GC_EVT_SWITCH_LONG_PRESS_VALUE, value);
        }
    }

    // ---- Validation différée des short clicks ----
    if (short_click_pending && (now - last_release_time >= SWITCH_MULTI_CLICK_MS)) {
        // La fenêtre pour les clics successifs est passée, on publie
        ESP_LOGI(TAG, "Click sequence complete: %d clicks", click_count);
        if (click_count > 0) {
            global_coordinator_publish(GC_EVT_SWITCH_SHORT_CLICK, click_count);
        }
        click_count = 0;
        short_click_pending = false;
    }
}

