#include "ir_led_manager.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "IR_LED_MANAGER";

static uint8_t g_pin_ir = 0;
static ir_config_t g_ir_config;
static bool g_is_enabled = false;

int ir_led_manager_init(uint8_t pin_ir, const ir_config_t *config)
{
    esp_err_t ret;
    
    if (config == NULL) {
        ESP_LOGE(TAG, "Config NULL");
        return -1;
    }
    
    g_pin_ir = pin_ir;
    g_ir_config = *config;
    
    // Configuration du GPIO IR (sortie GPIO simple, peut être amélioré avec PWM)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin_ir),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur configuration GPIO LED IR");
        return -1;
    }
    
    // Étteint la LED IR au démarrage
    gpio_set_level(pin_ir, 0);
    g_is_enabled = false;
    
    ESP_LOGI(TAG, "Gestionnaire LED IR initialisé (pin: %d, fréquence: %d Hz, duty: %d%%)",
             pin_ir, config->frequency, config->duty_cycle);
    
    return 0;
}

int ir_led_manager_send_command(const uint8_t *data, uint16_t length)
{
    if (data == NULL || length == 0) {
        ESP_LOGE(TAG, "Commande invalide");
        return -1;
    }
    
    ESP_LOGI(TAG, "Envoi commande IR (%d octets)", length);
    
    // TODO: Implémenter le protocole IR spécifique
    // Pour l'instant, c'est un placeholder
    // Vous devrez déterminer le protocole IR exact (NEC, Sony, Philips, etc.)
    
    return 0;
}

void ir_led_manager_enable(void)
{
    gpio_set_level(g_pin_ir, 1);
    g_is_enabled = true;
    ESP_LOGI(TAG, "LED IR activée");
}

void ir_led_manager_disable(void)
{
    gpio_set_level(g_pin_ir, 0);
    g_is_enabled = false;
    ESP_LOGI(TAG, "LED IR désactivée");
}

bool ir_led_manager_is_enabled(void)
{
    return g_is_enabled;
}
