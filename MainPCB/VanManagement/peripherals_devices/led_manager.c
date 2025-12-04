#include "led_manager.h"
#include "led_static_modes.h"
#include "led_dynamic_modes.h"
#include "gpio_pinout.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "LED_MGR";
static TaskHandle_t led_task_handle;
static led_strip_handle_t led_strips[LED_STRIP_COUNT];

// LED state struct
typedef struct {
    led_mode_type_t current_mode;
    uint8_t brightness;
    bool door_animation_active;
    bool error_animation_active;
    uint32_t animation_start_time;
} led_state_t;

static led_state_t roof_led_state;
static led_state_t ext_led_state;

// Internal helper
static led_state_t* get_led_state(led_strip_t strip) {
    return (strip <= LED_ROOF_STRIP_2) ? &roof_led_state : &ext_led_state;
}

// ---------------- Initialization ----------------
esp_err_t led_manager_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing LED manager...");

     // Configure exterior LED power pin
    gpio_config_t ext_cfg = {
        .pin_bit_mask = (1ULL << EXT_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&ext_cfg);
    if (ret != ESP_OK) return ret;

    // Turn off exterior LED
    gpio_set_level(EXT_LED, 0);

    // Initialize LED strips
    ret = led_static_init_strips(led_strips);
    if (ret != ESP_OK) return ret;

    // Initialize states
    memset(&roof_led_state, 0, sizeof(led_state_t));
    memset(&ext_led_state, 0, sizeof(led_state_t));
    roof_led_state.brightness = 255;
    ext_led_state.brightness = 255;

    // Start LED manager task pinned to CPU0 (BLE is on CPU1)
    // High priority to ensure smooth animations without interruptions
    BaseType_t res = xTaskCreatePinnedToCore(
        led_manager_task,
        "led_manager",
        4096,
        NULL,
        6,  // Priority 6 (higher than BLE priority 3)
        &led_task_handle,
        0   // CPU0 (BLE is on CPU1)
    );
    if (res != pdPASS) return ESP_FAIL;

    ESP_LOGI(TAG, "LED manager initialized");
    return ESP_OK;
}

// ---------------- Task ----------------
void led_manager_task(void *params)
{
    ESP_LOGI(TAG, "LED manager task started");
    while (1) {
        // Here we can process periodic tasks if needed
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ---------------- Mode Control ----------------
esp_err_t led_set_mode(led_strip_t strip, led_mode_type_t mode)
{
    led_state_t* state = get_led_state(strip);
    state->current_mode = mode;

    // Stop any dynamic animations
    led_dynamic_stop(strip);

    ESP_LOGI(TAG, "Setting LED mode %d for strip %d", mode, strip);
    switch(mode) {
        case LED_MODE_OFF:
            led_static_off(strip, state->brightness);
            heater_manager_set_air_heater(false, 0); // Provisoire: Stop air heater

            break;
        case LED_MODE_WHITE:
            led_static_white(strip, state->brightness);
            break;
        case LED_MODE_ORANGE:
            // led_static_orange(strip, state->brightness);
            // led_dynamic_door_open(strip, state->brightness, false); // Just for testing
            heater_manager_set_air_heater(true, 100); // Provisoire: Start air heater at 100% fan speed
            break;
        case LED_MODE_FILM:
            led_static_film(strip, state->brightness);
            break;
        case LED_MODE_RAINBOW:
            led_dynamic_rainbow(strip, state->brightness);
            break;
        case LED_MODE_DOOR_OPEN:
            led_dynamic_door_open(strip, state->brightness, true);
            if (strip <= LED_ROOF_STRIP_2) state->door_animation_active = true;
            break;
        case LED_MODE_DOOR_TIMEOUT:
            led_dynamic_door_open(strip, state->brightness, false);
            if (strip <= LED_ROOF_STRIP_2) state->door_animation_active = true;
            break;
        default:
            led_static_white(strip, state->brightness);
            break;
    }

    return ESP_OK;
}

esp_err_t led_set_brightness(led_strip_t strip, uint8_t brightness)
{
    led_state_t* state = get_led_state(strip);
    state->brightness = brightness;

    // Apply current mode with new brightness
    led_set_mode(strip, state->current_mode);
    return ESP_OK;
}

uint8_t led_get_brightness(led_strip_t strip)
{
    return get_led_state(strip)->brightness;
}

bool led_is_strip_on(led_strip_t strip)
{
    return get_led_state(strip)->current_mode != LED_MODE_OFF;
}

bool led_is_door_animation_active(void)
{
    return roof_led_state.door_animation_active;
}

void led_set_door_animation_active(bool active)
{
    roof_led_state.door_animation_active = active;
}

esp_err_t led_trigger_door_animation(void)
{
    roof_led_state.door_animation_active = true;
    roof_led_state.animation_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return ESP_OK;
}

esp_err_t led_trigger_error_mode(void)
{
    roof_led_state.error_animation_active = true;
    roof_led_state.animation_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return ESP_OK;
}

esp_err_t led_set_exterior_power(bool enabled)
{
    ESP_LOGI(TAG, "Setting exterior LED power to %d on GPIO %d", enabled, EXT_LED);
    gpio_reset_pin(EXT_LED);
    if(enabled){
        // The mosfet is pulled high by 5v power through a resistor so if the gpio is set to HIGH 3.3v 
        // it create a viltage divider and turns partially on the mosfet, so let the pin float to apply the 5V and completly turn on the mosfet.
        gpio_set_direction(EXT_LED, GPIO_MODE_INPUT); //(Hi-Z)
       
    } else {
        gpio_set_direction(EXT_LED, GPIO_MODE_OUTPUT);
        gpio_set_level(EXT_LED, 0);
    }
    return ESP_OK;
}


led_strip_handle_t led_manager_get_handle(led_strip_t strip)
{
    if (strip >= LED_STRIP_COUNT) return NULL;
    return led_strips[strip];
}

int led_manager_get_led_count(led_strip_t strip)
{
    switch (strip)
    {
        case LED_ROOF_STRIP_1: return LED_STRIP_1_COUNT;
        case LED_ROOF_STRIP_2: return LED_STRIP_2_COUNT;
        case LED_EXT_FRONT:    return LED_STRIP_EXT_FRONT_COUNT;
        case LED_EXT_BACK:     return LED_STRIP_EXT_BACK_COUNT;
        default: return 0;
    }
}

esp_err_t led_manager_update_van_state(van_state_t* van_state){
    if(!van_state) return ESP_ERR_INVALID_ARG;

    // Update roof LED state
    van_state->leds.leds_roof1.enabled = led_is_strip_on(LED_ROOF_STRIP_1);
    van_state->leds.leds_roof2.enabled = led_is_strip_on(LED_ROOF_STRIP_2);
    van_state->leds.leds_roof1.current_mode = roof_led_state.current_mode;
    van_state->leds.leds_roof2.current_mode = roof_led_state.current_mode;
    van_state->leds.leds_roof1.brightness = roof_led_state.brightness;
    van_state->leds.leds_roof2.brightness = roof_led_state.brightness;

    // Update exterior LED state
    van_state->leds.leds_av.enabled = led_is_strip_on(LED_EXT_FRONT);
    van_state->leds.leds_av.current_mode = ext_led_state.current_mode;
    van_state->leds.leds_av.brightness = ext_led_state.brightness;

    van_state->leds.leds_ar.enabled = led_is_strip_on(LED_EXT_BACK);
    van_state->leds.leds_ar.current_mode = ext_led_state.current_mode;
    van_state->leds.leds_ar.brightness = ext_led_state.brightness;

    return ESP_OK;
    
}