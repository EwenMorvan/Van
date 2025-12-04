#include "usb_manager.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "USB_MANAGER";

static uint8_t g_pin_usb_flag = 0;
static bool g_is_powered = false;
static usb_power_callback_t g_callback = NULL;

/**
 * @brief Tâche de monitoring d'alimentation USB
 * Vérifie régulièrement l'état du drapeau USB
 */
static void usb_monitor_task(void *pvParameters)
{
    bool last_state = false;
    
    while (1) {
        bool current_state = gpio_get_level(g_pin_usb_flag) == 1;
        
        // Détecte les changements d'état
        if (current_state != last_state) {
            g_is_powered = current_state;
            
            ESP_LOGI(TAG, "État alimentation USB: %s", g_is_powered ? "ALIMENTÉ" : "SANS ALIMENTATION");
            
            if (g_callback != NULL) {
                g_callback(g_is_powered);
            }
            
            last_state = current_state;
        }
        
        // Vérifie toutes les 100ms
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

int usb_manager_init(uint8_t pin_usb_flag, usb_power_callback_t callback)
{
    esp_err_t ret;
    
    g_pin_usb_flag = pin_usb_flag;
    g_callback = callback;
    
    // Configuration du GPIO pour le drapeau USB (entrée)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin_usb_flag),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // Polling plutôt que interruption
    };
    
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur configuration GPIO drapeau USB");
        return -1;
    }
    
    // Lit l'état initial
    g_is_powered = gpio_get_level(pin_usb_flag) == 1;
    
    // Crée une tâche de monitoring
    xTaskCreate(usb_monitor_task, "usb_monitor_task", 2048, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Gestionnaire USB initialisé (pin: %d, état initial: %s)", 
             pin_usb_flag, g_is_powered ? "ALIMENTÉ" : "SANS ALIMENTATION");
    
    return 0;
}

bool usb_manager_is_powered(void)
{
    return g_is_powered;
}

bool usb_manager_get_flag(void)
{
    return gpio_get_level(g_pin_usb_flag) == 1;
}
