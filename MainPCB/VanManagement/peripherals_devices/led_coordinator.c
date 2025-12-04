#include "../main/global_coordinator.h"
#include "led_manager.h"
#include "esp_log.h"

static const char *TAG = "LED_COORD";

// Variables pour suivre l'état actuel
static bool any_strip_on = false;

// Fonction helper pour vérifier si les strips sont allumés
static void update_strips_state(void)
{
    any_strip_on = led_is_strip_on(LED_ROOF_STRIP_1) || 
                   led_is_strip_on(LED_ROOF_STRIP_2);
}

static bool exterior_lights_on = false;

// Map click_count → led_mode_type_t
static led_mode_type_t map_click_to_mode(int click_count)
{
    switch (click_count)
    {
    case 1: return LED_MODE_OFF;        // Off
    case 2: return LED_MODE_WHITE;      // White/Default
    case 3: return LED_MODE_ORANGE;     // Orange
    case 4: return LED_MODE_WHITE;      // Mode spécial: allume intérieur + extérieur
    case 5: return LED_MODE_FILM;       // Film
    case 6: return LED_MODE_RAINBOW;    // Rainbow animation
    default: return LED_MODE_WHITE;     // Default fallback
    }
}

// Callback pour l'événement short click
static void led_short_click_cb(gc_event_t evt)
{
    ESP_LOGI(TAG, "LED coordinator received short click: %d", evt.value);
    led_mode_type_t mode = map_click_to_mode(evt.value);
    
    // Gestion spéciale du mode 4 (intérieur + extérieur)
    if (evt.value == 4) {
        // Allumer l'intérieur en blanc
        led_set_mode(LED_ROOF_STRIP_1, LED_MODE_WHITE);
        led_set_mode(LED_ROOF_STRIP_2, LED_MODE_WHITE);
        
        // Activer les LED extérieures
        exterior_lights_on = true;
        ESP_LOGI(TAG, "Activating exterior LEDs for mode 4");
        led_set_exterior_power(true);
        
        // Configurer les strips extérieurs si disponibles
        led_set_mode(LED_EXT_FRONT, LED_MODE_WHITE);
        led_set_mode(LED_EXT_BACK, LED_MODE_WHITE);
    } else {
        // Mode normal pour l'intérieur
        led_set_mode(LED_ROOF_STRIP_1, mode);
        led_set_mode(LED_ROOF_STRIP_2, mode);
        
        // Si ce n'est pas le mode 4, éteindre l'extérieur
        if (exterior_lights_on) {
            exterior_lights_on = false;
            // led_set_mode(LED_EXT_FRONT, LED_MODE_OFF);
            // led_set_mode(LED_EXT_BACK, LED_MODE_OFF);
            led_set_exterior_power(false);
            
        }
    }
    
    update_strips_state(); // Met à jour l'état des strips

    // Si les lumières sont allumées/eteintes manuellement, désactiver le mode porte
    if (any_strip_on || mode == LED_MODE_OFF) {
        led_set_door_animation_active(false);
    }
}

// Callback pour l'événement long press en cours
static void led_long_press_value_cb(gc_event_t evt)
{
    ESP_LOGI(TAG, "LED coordinator received long press value: %d", evt.value);
    
    // Si au moins une LED strip est allumée, ajuster la luminosité
    if (any_strip_on) {
        uint8_t brightness = (uint8_t)evt.value;
        led_set_brightness(LED_ROOF_STRIP_1, brightness);
        led_set_brightness(LED_ROOF_STRIP_2, brightness);
    }
}

// Callback pour l'événement fin de long press
static void led_long_press_final_cb(gc_event_t evt)
{
    ESP_LOGI(TAG, "LED coordinator received final long press value: %d", evt.value);
    
    // Si au moins une LED strip est allumée, fixer la luminosité finale
    if (any_strip_on) {
        uint8_t brightness = (uint8_t)evt.value;
        led_set_brightness(LED_ROOF_STRIP_1, brightness);
        led_set_brightness(LED_ROOF_STRIP_2, brightness);
    }
}

// Callback pour l'événement de changement d'état de la porte
static void door_light_value_changed_cb(gc_event_t evt)
{
    ESP_LOGI(TAG, "LED coordinator received door value changed: %d", evt.value);
    
    update_strips_state(); // Mettre à jour l'état avant de décider
    
    if(evt.value == 1) { // Porte ouverte
        if (!any_strip_on) {
            ESP_LOGI(TAG, "Door opened, lights were off, playing intro");
            led_set_mode(LED_ROOF_STRIP_1, LED_MODE_DOOR_OPEN);
            led_set_mode(LED_ROOF_STRIP_2, LED_MODE_DOOR_OPEN);
            led_set_door_animation_active(true);
        } else {
            ESP_LOGI(TAG, "Door opened, lights already on, setting flag for outro");
            led_set_door_animation_active(true);
        }
    } else { // Porte fermée
        if (led_is_door_animation_active()) {
            ESP_LOGI(TAG, "Timeout reached, door light active, playing outro");
            led_set_mode(LED_ROOF_STRIP_1, LED_MODE_DOOR_TIMEOUT);
            led_set_mode(LED_ROOF_STRIP_2, LED_MODE_DOOR_TIMEOUT);
            led_set_door_animation_active(false);
        } else {
            ESP_LOGI(TAG, "Timeout reached, door light not active, no outro");
        }
    }

    update_strips_state();
}

// Initialisation du module LED coordinator
esp_err_t led_coordinator_init(void)
{
    esp_err_t ret;

    // S'abonner aux événements de click court
    ret = global_coordinator_subscribe(GC_EVT_SWITCH_SHORT_CLICK, led_short_click_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to subscribe to short click events");
        return ret;
    }

    // S'abonner aux événements de long press (valeurs en cours)
    ret = global_coordinator_subscribe(GC_EVT_SWITCH_LONG_PRESS_VALUE, led_long_press_value_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to subscribe to long press value events");
        return ret;
    }

    // S'abonner aux événements de long press (valeur finale)
    ret = global_coordinator_subscribe(GC_EVT_SWITCH_LONG_PRESS_FINAL, led_long_press_final_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to subscribe to long press final events");
        return ret;
    }

    // S'abonner aux événements de changement d'état de la porte
    ret = global_coordinator_subscribe(GC_EVT_DOOR_VALUE_CHANGED, door_light_value_changed_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to subscribe to door value changed events");
        return ret;
    }

    // Initialiser l'état des strips
    update_strips_state();

    ESP_LOGI(TAG, "LED coordinator initialized");
    return ESP_OK;
}
