#include "fan_manager.h"
#include "communication_manager.h"
#include "gpio_pinout.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "FAN_MGR";
static TaskHandle_t fan_task_handle;

typedef struct {
    uint8_t elec_box_speed;
    uint8_t heater_fan_speed;
    bool hood_fan_active;
} fan_state_t;

static fan_state_t fan_state;

esp_err_t fan_manager_init(void) {
    esp_err_t ret;
    
    // Configure LEDC timer for PWM
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = FAN_PWM_RESOLUTION,
        .freq_hz = FAN_PWM_FREQUENCY,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer");
        return ret;
    }
    
    // Configure LEDC channel for electric box fan
    ledc_channel_config_t elec_box_channel = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = FAN_ELEC_BOX_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0
    };
    
    ret = ledc_channel_config(&elec_box_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure electric box fan channel");
        return ret;
    }
    
    // Configure LEDC channel for heater fan
    ledc_channel_config_t heater_channel = {
        .channel = LEDC_CHANNEL_1,
        .duty = 0,
        .gpio_num = FAN_HEATER_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0
    };
    
    ret = ledc_channel_config(&heater_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure heater fan channel");
        return ret;
    }
    
    // Configure hood fan control pin (digital on/off)
    gpio_config_t hood_fan_config = {
        .pin_bit_mask = (1ULL << HOOD_FAN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&hood_fan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure hood fan pin");
        return ret;
    }
    
    // Configure fan LED pin
    gpio_config_t fan_led_config = {
        .pin_bit_mask = (1ULL << LED_FAN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&fan_led_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure fan LED pin");
        return ret;
    }
    
    // Initialize fan state
    memset(&fan_state, 0, sizeof(fan_state_t));
    
    // Turn off all fans initially
    gpio_set_level(HOOD_FAN, 0);
    gpio_set_level(LED_FAN, 0);
    
    // Create fan manager task
    BaseType_t result = xTaskCreate(
        fan_manager_task,
        "fan_manager",
        4096,  // Increased stack size to match other managers
        NULL,
        2,
        &fan_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create fan task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Fan manager initialized");
    return ESP_OK;
}

esp_err_t fan_set_speed(fan_id_t fan, uint8_t speed_percent) {
    if (speed_percent > 100) {
        speed_percent = 100;
    }
    
    switch (fan) {
        case FAN_ELEC_BOX:
            fan_state.elec_box_speed = speed_percent;
            if (speed_percent > 0) {
                uint32_t duty = (speed_percent * ((1 << FAN_PWM_RESOLUTION) - 1)) / 100;
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                ESP_LOGI(TAG, "Electric box fan speed set to %d%%", speed_percent);
            } else {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                ESP_LOGI(TAG, "Electric box fan stopped");
            }
            break;
            
        case FAN_HEATER:
            fan_state.heater_fan_speed = speed_percent;
            if (speed_percent > 0) {
                uint32_t duty = (speed_percent * ((1 << FAN_PWM_RESOLUTION) - 1)) / 100;
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
                
                // Turn on fan LEDs when heater fan is running
                gpio_set_level(LED_FAN, 1);
                ESP_LOGI(TAG, "Heater fan speed set to %d%%", speed_percent);
            } else {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
                gpio_set_level(LED_FAN, 0);
                ESP_LOGI(TAG, "Heater fan stopped");
            }
            break;
            
        case FAN_HOOD:
            fan_state.hood_fan_active = (speed_percent > 0);
            gpio_set_level(HOOD_FAN, fan_state.hood_fan_active ? 1 : 0);
            ESP_LOGI(TAG, "Hood fan %s", fan_state.hood_fan_active ? "started" : "stopped");
            break;
            
        default:
            ESP_LOGW(TAG, "Invalid fan ID: %d", fan);
            return ESP_ERR_INVALID_ARG;
    }
    
    return ESP_OK;
}

esp_err_t fan_start(fan_id_t fan) {
    return fan_set_speed(fan, 100); // Start at full speed
}

esp_err_t fan_stop(fan_id_t fan) {
    return fan_set_speed(fan, 0);
}

uint8_t fan_get_speed(fan_id_t fan) {
    switch (fan) {
        case FAN_ELEC_BOX:
            return fan_state.elec_box_speed;
        case FAN_HEATER:
            return fan_state.heater_fan_speed;
        case FAN_HOOD:
            return fan_state.hood_fan_active ? 100 : 0;
        default:
            return 0;
    }
}

esp_err_t fan_manager_set_hood_state(bool enabled) {
    ESP_LOGI(TAG, "Setting hood fan state to %s", enabled ? "ON" : "OFF");
    
    if (enabled) {
        // Turn on hood fan at full speed (100%)
        esp_err_t ret = fan_set_speed(FAN_HOOD, 100);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Hood fan turned ON at 100% speed");
        } else {
            ESP_LOGE(TAG, "Failed to turn on hood fan: %s", esp_err_to_name(ret));
        }
        return ret;
    } else {
        // Turn off hood fan
        esp_err_t ret = fan_stop(FAN_HOOD);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Hood fan turned OFF");
        } else {
            ESP_LOGE(TAG, "Failed to turn off hood fan: %s", esp_err_to_name(ret));
        }
        return ret;
    }
}

void fan_manager_task(void *parameters) {
    ESP_LOGI(TAG, "Fan manager task started");
    
    while (1) {
        // Send fan state to communication manager
        comm_send_message(COMM_MSG_FAN_UPDATE, &fan_state, sizeof(fan_state_t));
        
        // Monitor system temperatures and adjust electric box fan accordingly
        van_state_t *state = protocol_get_state();
        if (state) {
            // Auto-control electric box fan based on system conditions
            if (state->sensors.onboard_temperature > 45.0f) {
                // High temperature, run fan at high speed
                if (fan_state.elec_box_speed < 80) {
                    fan_set_speed(FAN_ELEC_BOX, 80);
                }
            } else if (state->sensors.onboard_temperature > 35.0f) {
                // Medium temperature, run fan at medium speed
                if (fan_state.elec_box_speed < 50) {
                    fan_set_speed(FAN_ELEC_BOX, 50);
                }
            } else if (state->sensors.onboard_temperature < 30.0f) {
                // Cool temperature, turn off fan
                if (fan_state.elec_box_speed > 0) {
                    fan_set_speed(FAN_ELEC_BOX, 0);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // Update every 5 seconds
    }
}
