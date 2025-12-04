

#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "esp_err.h"
#include "led_strip.h"
#include <stdint.h>
#include <stdbool.h>
#include "../communications/protocol.h"
#include "heater_manager.h"// provisoir à enlever plus tard


typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_WHITE,
    LED_MODE_ORANGE,
    LED_MODE_FAN,
    LED_MODE_FILM,
    LED_MODE_RAINBOW,
    LED_MODE_DOOR_OPEN,
    LED_MODE_DOOR_TIMEOUT,
} led_mode_type_t;

typedef enum {
    LED_ROOF_STRIP_1,
    LED_ROOF_STRIP_2,
    LED_EXT_FRONT,
    LED_EXT_BACK,
    LED_STRIP_COUNT
} led_strip_t;
esp_err_t led_manager_init(void);

// Set LED mode for a specific strip
esp_err_t led_set_mode(led_strip_t strip, led_mode_type_t mode);

// Set LED brightness (0-255)
esp_err_t led_set_brightness(led_strip_t strip, uint8_t brightness);

// Trigger pre-defined animations
esp_err_t led_trigger_door_animation(void);
esp_err_t led_trigger_error_mode(void);

// Control exterior LED power
esp_err_t led_set_exterior_power(bool enabled);

// Get current brightness (0-255)
uint8_t led_get_brightness(led_strip_t strip);

// Check if a strip is currently on (not OFF mode)
bool led_is_strip_on(led_strip_t strip);

// Check if door animation is active
bool led_is_door_animation_active(void);

// Set door animation active state
void led_set_door_animation_active(bool active);

void led_manager_task(void *params);

led_strip_handle_t led_manager_get_handle(led_strip_t strip);
int led_manager_get_led_count(led_strip_t strip);

esp_err_t led_manager_update_van_state(van_state_t* van_state);

#endif // LED_MANAGER_H