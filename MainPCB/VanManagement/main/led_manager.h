#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "esp_err.h"
#include "protocol.h"
#include "log_level.h"

#define LED_SWITCH_DEBOUNCE_MS 50
#define LED_SHORT_PRESS_MS 500
#define LED_LONG_PRESS_MS 1000
#define LED_DOOR_ANIMATION_MS 4000
#define LED_DOOR_FADE_MS 20000
#define LED_ERROR_FLASH_COUNT 5

typedef enum {
    LED_ROOF_STRIP_1,
    LED_ROOF_STRIP_2,
    LED_EXT_FRONT,
    LED_EXT_BACK
} led_strip_t;

typedef enum {
    LED_MODE_DEFAULT,
    LED_MODE_CUSTOM_1,
    LED_MODE_CUSTOM_2,
    LED_MODE_CUSTOM_3,
    LED_MODE_CUSTOM_4,
    LED_MODE_DOOR_OPEN,
    LED_MODE_ERROR,
    LED_MODE_REALTIME
} led_mode_type_t;

esp_err_t led_manager_init(void);
void led_manager_task(void *parameters);
esp_err_t led_set_mode(led_strip_t strip, led_mode_type_t mode);
esp_err_t led_set_brightness(led_strip_t strip, uint8_t brightness);
esp_err_t led_trigger_door_animation(void);
esp_err_t led_trigger_error_mode(void);
esp_err_t led_set_exterior_power(bool enabled);


#endif // LED_MANAGER_H
