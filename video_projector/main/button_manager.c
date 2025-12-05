#include "button_manager.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BUTTON_MANAGER";

static uint8_t g_pin_button = 0;
static uint8_t g_pin_led = 0;
static button_callback_t g_callback = NULL;
static bool g_led_state = false;
static uint32_t g_last_press_time = 0;
static bool g_is_pressed = false;

#define LONG_PRESS_TIME_MS 1000
#define DEBOUNCE_TIME_MS 50

/**
 * @brief Tâche de gestion du bouton (debounce + détection long/short press)
 */
static void button_task(void *pvParameters)
{
    while (1) {
        int button_level = gpio_get_level(g_pin_button);
        
        if (button_level == 0 && !g_is_pressed) {  // Bouton appuyé
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
            
            if (gpio_get_level(g_pin_button) == 0) {
                g_is_pressed = true;
                g_last_press_time = xTaskGetTickCount();
                ESP_LOGI(TAG, "Bouton appuyé");
            }
        }
        else if (button_level == 1 && g_is_pressed) {  // Bouton relâché
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
            
            if (gpio_get_level(g_pin_button) == 1) {
                uint32_t press_duration = (xTaskGetTickCount() - g_last_press_time) * 
                                          portTICK_PERIOD_MS;
                
                ESP_LOGI(TAG, "Bouton relâché après %lu ms", press_duration);
                
                if (g_callback != NULL) {
                    if (press_duration >= LONG_PRESS_TIME_MS) {
                        g_callback(BUTTON_EVENT_LONG_PRESS);
                    } else {
                        g_callback(BUTTON_EVENT_SHORT_PRESS);
                    }
                    g_callback(BUTTON_EVENT_RELEASED);
                }
                
                g_is_pressed = false;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int button_manager_init(uint8_t pin_button, uint8_t pin_led, button_callback_t callback)
{
    esp_err_t ret;
    
    g_pin_button = pin_button;
    g_pin_led = pin_led;
    g_callback = callback;
    
    // Configuration du GPIO bouton (entrée)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin_button),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Pull-up interne
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // Pas d'interruption, utilisation d'une tâche
    };
    
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur configuration GPIO bouton");
        return -1;
    }
    
    // Configuration du GPIO LED (sortie)
    io_conf.pin_bit_mask = (1ULL << pin_led);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur configuration GPIO LED");
        return -1;
    }
    
    // Éteint la LED au démarrage
    gpio_set_level(pin_led, 0);
    g_led_state = false;
    
    // Crée une tâche pour gérer le bouton
    xTaskCreate(button_task, "button_task", 4096, NULL, 10, NULL);
    
    ESP_LOGI(TAG, "Gestionnaire bouton initialisé (bouton: %d, LED: %d)", pin_button, pin_led);
    return 0;
}

void button_manager_set_led(bool enabled)
{
    gpio_set_level(g_pin_led, enabled ? 1 : 0);
    g_led_state = enabled;
}

bool button_manager_get_led_state(void)
{
    return g_led_state;
}

void button_manager_isr_handler(void)
{
    // Cette fonction n'est pas utilisée dans cette implémentation
    // car nous utilisons une tâche pour gérer le debounce
}
