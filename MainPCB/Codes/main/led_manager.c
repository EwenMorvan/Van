#include "led_manager.h"
#include "communication_manager.h"
#include "sensor_manager.h"
#include "gpio_pinout.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "LED_MGR";
static TaskHandle_t led_task_handle;
static led_strip_handle_t led_strips[4];

// LED state tracking
typedef struct {
    uint8_t current_mode;
    uint8_t brightness;
    bool switch_pressed;
    uint32_t switch_press_time;
    uint32_t last_switch_time;
    uint8_t click_count;
    bool door_animation_active;
    bool error_animation_active;
    uint32_t animation_start_time;
} led_state_t;

static led_state_t roof_led_state;
static led_state_t ext_led_state;

esp_err_t led_manager_init(void) {
    esp_err_t ret;
    
    // Configure exterior LED power control pin
    gpio_config_t ext_power_config = {
        .pin_bit_mask = (1ULL << EXT_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&ext_power_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure exterior LED power pin");
        return ret;
    }
    
    // Configure switch input pin
    gpio_config_t switch_config = {
        .pin_bit_mask = (1ULL << INTER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&switch_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure switch input pin");
        return ret;
    }
    
    // Initialize LED strips
    led_strip_config_t strip_config = {
        .strip_gpio_num = DI_LED1,
        .max_leds = LED_STRIP_1_COUNT,
        .led_pixel_format = LED_PIXEL_FORMAT_GRBW,
        .led_model = LED_MODEL_SK6812,
        .flags.invert_out = false
    };
    
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false
    };
    
    // LED Strip 1 (Roof)
    strip_config.strip_gpio_num = DI_LED1;
    strip_config.max_leds = LED_STRIP_1_COUNT;
    ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strips[LED_ROOF_STRIP_1]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip 1");
        return ret;
    }
    
    // LED Strip 2 (Roof)
    strip_config.strip_gpio_num = DI_LED2;
    strip_config.max_leds = LED_STRIP_2_COUNT;
    ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strips[LED_ROOF_STRIP_2]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip 2");
        return ret;
    }
    
    // LED Strip Exterior Front
    strip_config.strip_gpio_num = DI_LED_AV;
    strip_config.max_leds = LED_STRIP_EXT_FRONT_COUNT;
    ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strips[LED_EXT_FRONT]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create exterior front LED strip");
        return ret;
    }
    
    // LED Strip Exterior Back
    strip_config.strip_gpio_num = DI_LED_AR;
    strip_config.max_leds = LED_STRIP_EXT_BACK_COUNT;
    ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strips[LED_EXT_BACK]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create exterior back LED strip");
        return ret;
    }
    
    // Initialize LED states
    memset(&roof_led_state, 0, sizeof(led_state_t));
    memset(&ext_led_state, 0, sizeof(led_state_t));
    roof_led_state.brightness = 255;
    ext_led_state.brightness = 255;
    
    // Turn off exterior LED power initially
    gpio_set_level(EXT_LED, 0);
    
    // Create LED manager task
    BaseType_t result = xTaskCreate(
        led_manager_task,
        "led_manager",
        4096,
        NULL,
        4,
        &led_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED manager task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "LED manager initialized");
    return ESP_OK;
}

static void set_default_mode(led_strip_t strip) {
    int max_leds = (strip == LED_ROOF_STRIP_1) ? LED_STRIP_1_COUNT :
                   (strip == LED_ROOF_STRIP_2) ? LED_STRIP_2_COUNT :
                   (strip == LED_EXT_FRONT) ? LED_STRIP_EXT_FRONT_COUNT :
                   LED_STRIP_EXT_BACK_COUNT;
    
    uint8_t brightness = (strip <= LED_ROOF_STRIP_2) ? roof_led_state.brightness : ext_led_state.brightness;
    
    // Set all LEDs to natural white
    for (int i = 0; i < max_leds; i++) {
        led_strip_set_pixel_rgbw(led_strips[strip], i, 0, 0, 0, brightness);
    }
    led_strip_refresh(led_strips[strip]);
}

static void door_open_animation(void) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed = current_time - roof_led_state.animation_start_time;
    
    if (elapsed < LED_DOOR_ANIMATION_MS) {
        // Moving dot animation
        int position = (elapsed * LED_STRIP_1_COUNT) / LED_DOOR_ANIMATION_MS;
        uint8_t brightness = roof_led_state.brightness * 25 / 100; // 25% brightness
        
        // Clear strips
        led_strip_clear(led_strips[LED_ROOF_STRIP_1]);
        led_strip_clear(led_strips[LED_ROOF_STRIP_2]);
        
        // Set moving dot and persistent LEDs
        for (int i = 0; i <= position; i++) {
            if (i < LED_STRIP_1_COUNT) {
                led_strip_set_pixel_rgbw(led_strips[LED_ROOF_STRIP_1], LED_STRIP_1_COUNT - 1 - i, 0, 0, 0, brightness);
                led_strip_set_pixel_rgbw(led_strips[LED_ROOF_STRIP_2], LED_STRIP_2_COUNT - 1 - i, 0, 0, 0, brightness);
            }
        }
        
        led_strip_refresh(led_strips[LED_ROOF_STRIP_1]);
        led_strip_refresh(led_strips[LED_ROOF_STRIP_2]);
    } else {
        // Animation complete, set full brightness
        roof_led_state.door_animation_active = false;
        set_default_mode(LED_ROOF_STRIP_1);
        set_default_mode(LED_ROOF_STRIP_2);
    }
}

static void error_animation(void) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed = current_time - roof_led_state.animation_start_time;
    
    // Flash red LEDs 5 times (500ms per flash)
    if (elapsed < LED_ERROR_FLASH_COUNT * 500) {
        bool flash_on = (elapsed / 250) % 2 == 0;
        
        // Clear strips
        led_strip_clear(led_strips[LED_ROOF_STRIP_1]);
        led_strip_clear(led_strips[LED_ROOF_STRIP_2]);
        
        if (flash_on) {
            // Turn on every other LED in red
            for (int i = 0; i < LED_STRIP_1_COUNT; i += 2) {
                led_strip_set_pixel_rgbw(led_strips[LED_ROOF_STRIP_1], i, 255, 0, 0, 0);
            }
            for (int i = 0; i < LED_STRIP_2_COUNT; i += 2) {
                led_strip_set_pixel_rgbw(led_strips[LED_ROOF_STRIP_2], i, 255, 0, 0, 0);
            }
        }
        
        led_strip_refresh(led_strips[LED_ROOF_STRIP_1]);
        led_strip_refresh(led_strips[LED_ROOF_STRIP_2]);
    } else {
        // Error animation complete
        roof_led_state.error_animation_active = false;
        set_default_mode(LED_ROOF_STRIP_1);
        set_default_mode(LED_ROOF_STRIP_2);
    }
}

static void handle_switch_input(void) {
    bool switch_state = !gpio_get_level(INTER); // Inverted due to pull-up
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Debounce switch
    static uint32_t last_debounce_time = 0;
    if (current_time - last_debounce_time < LED_SWITCH_DEBOUNCE_MS) {
        return;
    }
    
    static bool prev_switch_state = false;
    
    if (switch_state && !prev_switch_state) {
        // Switch pressed
        roof_led_state.switch_pressed = true;
        roof_led_state.switch_press_time = current_time;
        last_debounce_time = current_time;
    } else if (!switch_state && prev_switch_state) {
        // Switch released
        if (roof_led_state.switch_pressed) {
            uint32_t press_duration = current_time - roof_led_state.switch_press_time;
            
            if (press_duration >= LED_LONG_PRESS_MS) {
                // Long press - adjust brightness
                ESP_LOGI(TAG, "Long press detected");
                // Brightness adjustment logic would be implemented here
            } else {
                // Short press - count clicks
                if (current_time - roof_led_state.last_switch_time < 500) {
                    roof_led_state.click_count++;
                } else {
                    roof_led_state.click_count = 1;
                }
                roof_led_state.last_switch_time = current_time;
                
                ESP_LOGI(TAG, "Short press detected, click count: %d", roof_led_state.click_count);
                
                // Handle mode change based on click count
                if (roof_led_state.click_count <= MAX_LED_MODES) {
                    roof_led_state.current_mode = roof_led_state.click_count - 1;
                    set_default_mode(LED_ROOF_STRIP_1);
                    set_default_mode(LED_ROOF_STRIP_2);
                }
            }
            
            roof_led_state.switch_pressed = false;
        }
        last_debounce_time = current_time;
    }
    
    prev_switch_state = switch_state;
}

void led_manager_task(void *parameters) {
    ESP_LOGI(TAG, "LED manager task started");
    
    while (1) {
        // Handle switch input for roof LEDs
        handle_switch_input();
        
        // Handle door open animation
        if (roof_led_state.door_animation_active) {
            door_open_animation();
        }
        
        // Handle error animation
        if (roof_led_state.error_animation_active) {
            error_animation();
        }
        
        // Check for door open condition
        if (sensor_is_door_open() && !roof_led_state.door_animation_active && !roof_led_state.error_animation_active) {
            led_trigger_door_animation();
        }
        
        // Send LED state to communication manager
        struct {
            typeof(roof_led_state) roof;
            typeof(ext_led_state) exterior;
            bool error_mode_active;
        } led_data = {
            .roof = roof_led_state,
            .exterior = ext_led_state,
            .error_mode_active = roof_led_state.error_animation_active
        };
        
        comm_send_message(COMM_MSG_LED_UPDATE, &led_data, sizeof(led_data));
        
        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz update rate
    }
}

esp_err_t led_set_mode(led_strip_t strip, led_mode_type_t mode) {
    if (strip <= LED_ROOF_STRIP_2) {
        roof_led_state.current_mode = mode;
    } else {
        ext_led_state.current_mode = mode;
    }
    
    set_default_mode(strip);
    return ESP_OK;
}

esp_err_t led_set_brightness(led_strip_t strip, uint8_t brightness) {
    if (strip <= LED_ROOF_STRIP_2) {
        roof_led_state.brightness = brightness;
    } else {
        ext_led_state.brightness = brightness;
    }
    
    set_default_mode(strip);
    return ESP_OK;
}

esp_err_t led_trigger_door_animation(void) {
    roof_led_state.door_animation_active = true;
    roof_led_state.animation_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "Door open animation triggered");
    return ESP_OK;
}

esp_err_t led_trigger_error_mode(void) {
    roof_led_state.error_animation_active = true;
    roof_led_state.animation_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "Error animation triggered");
    return ESP_OK;
}

esp_err_t led_set_exterior_power(bool enabled) {
    gpio_set_level(EXT_LED, enabled ? 1 : 0);
    ESP_LOGI(TAG, "Exterior LED power %s", enabled ? "enabled" : "disabled");
    return ESP_OK;
}
