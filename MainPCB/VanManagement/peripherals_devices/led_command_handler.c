#include "led_command_handler.h"
#include "led_manager.h"
#include "led_static_modes.h"
#include "led_dynamic_modes.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "LED_CMD_HANDLER";

// Task handles for custom dynamic animations
static TaskHandle_t custom_animation_tasks[LED_STRIP_COUNT] = {NULL};

// Structure to hold custom animation parameters
typedef struct {
    led_strip_t strip;
    led_dynamic_command_t* dynamic_cmd;
    volatile bool stop_requested;
} custom_animation_task_t;

static custom_animation_task_t custom_animation_params[LED_STRIP_COUNT];

// Helper function to convert led_strip_static_target_t to led_strip_t enum(s)
static void map_static_target_to_strips(led_strip_static_target_t target, 
                                        led_strip_t* strips, 
                                        int* strip_count) {
    *strip_count = 0;
    
    switch (target) {
        case ROOF_LED1:
            strips[0] = LED_ROOF_STRIP_1;
            *strip_count = 1;
            break;
            
        case ROOF_LED2:
            strips[0] = LED_ROOF_STRIP_2;
            *strip_count = 1;
            break;
            
        case ROOF_LED_ALL:
            strips[0] = LED_ROOF_STRIP_1;
            strips[1] = LED_ROOF_STRIP_2;
            *strip_count = 2;
            break;
            
        case EXT_AV_LED:
            strips[0] = LED_EXT_FRONT;
            *strip_count = 1;
            break;
            
        case EXT_AR_LED:
            strips[0] = LED_EXT_BACK;
            *strip_count = 1;
            break;
            
        case EXT_LED_ALL:
            strips[0] = LED_EXT_FRONT;
            strips[1] = LED_EXT_BACK;
            *strip_count = 2;
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown static target: %d", target);
            break;
    }
}

// Helper function to convert led_strip_dynamic_target_t to led_strip_t enum(s)
static void map_dynamic_target_to_strips(led_strip_dynamic_target_t target,
                                         led_strip_t* strips,
                                         int* strip_count) {
    *strip_count = 0;
    
    switch (target) {
        case ROOF_LED1_DYNAMIC:
            strips[0] = LED_ROOF_STRIP_1;
            *strip_count = 1;
            break;
            
        case ROOF_LED2_DYNAMIC:
            strips[0] = LED_ROOF_STRIP_2;
            *strip_count = 1;
            break;
            
        case ROOF_LED_ALL_DYNAMIC:
            strips[0] = LED_ROOF_STRIP_1;
            strips[1] = LED_ROOF_STRIP_2;
            *strip_count = 2;
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown dynamic target: %d", target);
            break;
    }
}

// Helper function to apply a single LED color to a specific LED in a strip
static void apply_led_color(led_strip_handle_t handle, int led_index, const led_data_t* color) {
    if (!handle || !color) return;
    
    // Scale RGBW by brightness
    float scale = color->brightness / 255.0f;
    
    led_strip_set_pixel_rgbw(handle, led_index,
                             (uint8_t)(color->r * scale),
                             (uint8_t)(color->g * scale),
                             (uint8_t)(color->b * scale),
                             (uint8_t)(color->w * scale));
}

// Helper function to interpolate between two colors
static void interpolate_colors(const led_data_t* color1, 
                               const led_data_t* color2,
                               float ratio,
                               led_data_t* result) {
    result->r = (uint8_t)(color1->r + (color2->r - color1->r) * ratio);
    result->g = (uint8_t)(color1->g + (color2->g - color1->g) * ratio);
    result->b = (uint8_t)(color1->b + (color2->b - color1->b) * ratio);
    result->w = (uint8_t)(color1->w + (color2->w - color1->w) * ratio);
    result->brightness = (uint8_t)(color1->brightness + (color2->brightness - color1->brightness) * ratio);
}

// Helper function for ease-in-out interpolation
static float ease_in_out(float t) {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

// Apply static LED command
static esp_err_t apply_static_command(const led_static_command_t* static_cmd) {
    if (!static_cmd) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Applying static LED command, target=%d", static_cmd->strip_target);
    
    led_strip_t strips[2];
    int strip_count;
    map_static_target_to_strips(static_cmd->strip_target, strips, &strip_count);
    
    if (strip_count == 0) {
        ESP_LOGE(TAG, "No strips mapped for static target");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Stop any running animations on these strips
    for (int s = 0; s < strip_count; s++) {
        led_dynamic_stop(strips[s]);
        if (custom_animation_tasks[strips[s]] != NULL) {
            custom_animation_params[strips[s]].stop_requested = true;
            vTaskDelay(pdMS_TO_TICKS(50)); // Give task time to stop
        }
    }
    
    // Apply colors to each strip
    for (int s = 0; s < strip_count; s++) {
        led_strip_t strip = strips[s];
        led_strip_handle_t handle = led_manager_get_handle(strip);
        int num_leds = led_manager_get_led_count(strip);
        
        if (!handle || num_leds <= 0) {
            ESP_LOGE(TAG, "Invalid handle or LED count for strip %d", strip);
            continue;
        }
        
        // Get the appropriate color array based on target
        const led_data_t* colors = NULL;
        int color_count = 0;
        
        // Determine which color array to use
        if (static_cmd->strip_target == ROOF_LED1 || static_cmd->strip_target == ROOF_LED_ALL) {
            if (strip == LED_ROOF_STRIP_1) {
                colors = static_cmd->colors.roof.roof1_colors.color;
                color_count = LED_STRIP_1_COUNT;
            }
        }
        
        if (static_cmd->strip_target == ROOF_LED2 || static_cmd->strip_target == ROOF_LED_ALL) {
            if (strip == LED_ROOF_STRIP_2) {
                colors = static_cmd->colors.roof.roof2_colors.color;
                color_count = LED_STRIP_2_COUNT;
            }
        }
        
        if (static_cmd->strip_target == EXT_AV_LED || static_cmd->strip_target == EXT_LED_ALL) {
            if (strip == LED_EXT_FRONT) {
                colors = static_cmd->colors.ext.ext_av_colors.color;
                color_count = LED_STRIP_EXT_FRONT_COUNT;
            }
        }
        
        if (static_cmd->strip_target == EXT_AR_LED || static_cmd->strip_target == EXT_LED_ALL) {
            if (strip == LED_EXT_BACK) {
                colors = static_cmd->colors.ext.ext_ar_colors.color;
                color_count = LED_STRIP_EXT_BACK_COUNT;
            }
        }
        
        if (!colors || color_count == 0) {
            ESP_LOGE(TAG, "No color data for strip %d", strip);
            continue;
        }
        
        // Apply each LED color
        ESP_LOGI(TAG, "Applying %d colors to strip %d", color_count, strip);
        for (int i = 0; i < num_leds && i < color_count; i++) {
            apply_led_color(handle, i, &colors[i]);
        }
        
        // Refresh the strip to show the changes
        esp_err_t ret = led_strip_refresh(handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to refresh strip %d: %s", strip, esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Strip %d refreshed successfully", strip);
        }
        
        // Enable exterior power if needed
        if (strip == LED_EXT_FRONT || strip == LED_EXT_BACK) {
            led_set_exterior_power(true);
        }
    }
    
    return ESP_OK;
}

// Custom animation task for dynamic LED commands
static void custom_animation_task(void *param) {
    custom_animation_task_t* task_params = (custom_animation_task_t*)param;
    if (!task_params || !task_params->dynamic_cmd) {
        ESP_LOGE(TAG, "Invalid task parameters");
        vTaskDelete(NULL);
        return;
    }
    
    led_strip_t strip = task_params->strip;
    led_dynamic_command_t* cmd = task_params->dynamic_cmd;
    
    led_strip_handle_t handle = led_manager_get_handle(strip);
    int num_leds = led_manager_get_led_count(strip);
    
    if (!handle || num_leds <= 0) {
        ESP_LOGE(TAG, "Invalid handle or LED count for strip %d", strip);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Starting custom animation on strip %d: %d keyframes, %dms duration, loop=%d",
             strip, cmd->keyframe_count, cmd->loop_duration_ms, cmd->loop_behavior);
    
    // Debug: Print keyframe timestamps and first color values
    // ESP_LOGI(TAG, "Keyframe timestamps for strip %d (target=%d):", strip, cmd->strip_target);
    // for (int i = 0; i < cmd->keyframe_count; i++) {
    //     led_keyframe_t* kf = &cmd->keyframes[i];
    //     ESP_LOGI(TAG, "  KF[%d]: %dms, transition=%d", i, 
    //              kf->timestamp_ms, kf->transition);
        
    //     // Debug: Print first LED color from this keyframe
    //     if (cmd->strip_target == ROOF_LED1_DYNAMIC && strip == LED_ROOF_STRIP_1) {
    //         led_data_t* c = &kf->colors.roof1.color[0];
    //         ESP_LOGI(TAG, "    First LED: R=%d G=%d B=%d W=%d Bright=%d", 
    //                  c->r, c->g, c->b, c->w, c->brightness);
    //     } else if (cmd->strip_target == ROOF_LED2_DYNAMIC && strip == LED_ROOF_STRIP_2) {
    //         led_data_t* c = &kf->colors.roof2.color[0];
    //         ESP_LOGI(TAG, "    First LED: R=%d G=%d B=%d W=%d Bright=%d", 
    //                  c->r, c->g, c->b, c->w, c->brightness);
    //     } else if (cmd->strip_target == ROOF_LED_ALL_DYNAMIC) {
    //         if (strip == LED_ROOF_STRIP_1) {
    //             led_data_t* c = &kf->colors.both.roof1.color[0];
    //             ESP_LOGI(TAG, "    First LED (BOTH.ROOF1): R=%d G=%d B=%d W=%d Bright=%d", 
    //                      c->r, c->g, c->b, c->w, c->brightness);
    //         } else if (strip == LED_ROOF_STRIP_2) {
    //             led_data_t* c = &kf->colors.both.roof2.color[0];
    //             ESP_LOGI(TAG, "    First LED (BOTH.ROOF2): R=%d G=%d B=%d W=%d Bright=%d", 
    //                      c->r, c->g, c->b, c->w, c->brightness);
    //         }
    //     }
    // }
    
    uint32_t animation_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bool forward = true;
    
    while (!task_params->stop_requested) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t elapsed = current_time - animation_start;
        
        // Calculate position in animation based on loop behavior
        uint32_t position_in_loop = elapsed % cmd->loop_duration_ms;
        
        if (cmd->loop_behavior == LOOP_BEHAVIOR_PING_PONG) {
            uint32_t cycle = (elapsed / cmd->loop_duration_ms) % 2;
            if (cycle == 1) { // Reverse direction
                position_in_loop = cmd->loop_duration_ms - position_in_loop;
            }
        } else if (cmd->loop_behavior == LOOP_BEHAVIOR_ONCE) {
            if (elapsed >= cmd->loop_duration_ms) {
                // Animation finished
                break;
            }
        }
        
        // Find the two keyframes to interpolate between
        int keyframe_index = 0;
        for (int i = 0; i < cmd->keyframe_count - 1; i++) {
            if (position_in_loop >= cmd->keyframes[i].timestamp_ms &&
                position_in_loop < cmd->keyframes[i + 1].timestamp_ms) {
                keyframe_index = i;
                break;
            }
        }
        
        // Get the two keyframes
        led_keyframe_t* kf1 = &cmd->keyframes[keyframe_index];
        led_keyframe_t* kf2 = (keyframe_index < cmd->keyframe_count - 1) 
                              ? &cmd->keyframes[keyframe_index + 1] 
                              : &cmd->keyframes[0];
        
        // Calculate interpolation ratio
        float ratio = 0.0f;
        if (kf2->timestamp_ms > kf1->timestamp_ms) {
            ratio = (float)(position_in_loop - kf1->timestamp_ms) / 
                    (float)(kf2->timestamp_ms - kf1->timestamp_ms);
        }
        
        // Apply transition function
        if (kf1->transition == TRANSITION_EASE_IN_OUT) {
            ratio = ease_in_out(ratio);
        } else if (kf1->transition == TRANSITION_STEP) {
            ratio = 0.0f; // Stay at first keyframe
        }
        
        // Get color arrays based on strip target
        const led_data_t* colors1 = NULL;
        const led_data_t* colors2 = NULL;
        int expected_led_count = 0;
        
        if (cmd->strip_target == ROOF_LED1_DYNAMIC) {
            if (strip == LED_ROOF_STRIP_1) {
                colors1 = kf1->colors.roof1.color;
                colors2 = kf2->colors.roof1.color;
                expected_led_count = LED_STRIP_1_COUNT;
                ESP_LOGD(TAG, "Using ROOF1 colors for strip %d", strip);
            }
        } else if (cmd->strip_target == ROOF_LED2_DYNAMIC) {
            if (strip == LED_ROOF_STRIP_2) {
                colors1 = kf1->colors.roof2.color;
                colors2 = kf2->colors.roof2.color;
                expected_led_count = LED_STRIP_2_COUNT;
                ESP_LOGD(TAG, "Using ROOF2 colors for strip %d", strip);
            }
        } else if (cmd->strip_target == ROOF_LED_ALL_DYNAMIC) {
            if (strip == LED_ROOF_STRIP_1) {
                colors1 = kf1->colors.both.roof1.color;
                colors2 = kf2->colors.both.roof1.color;
                expected_led_count = LED_STRIP_1_COUNT;
                ESP_LOGD(TAG, "Using BOTH.ROOF1 colors for strip %d", strip);
            } else if (strip == LED_ROOF_STRIP_2) {
                colors1 = kf1->colors.both.roof2.color;
                colors2 = kf2->colors.both.roof2.color;
                expected_led_count = LED_STRIP_2_COUNT;
                ESP_LOGD(TAG, "Using BOTH.ROOF2 colors for strip %d", strip);
            }
        }
        
        if (!colors1 || !colors2) {
            ESP_LOGE(TAG, "No color data for animation (target=%d, strip=%d)", 
                     cmd->strip_target, strip);
            ESP_LOGE(TAG, "kf1=%p, kf2=%p, colors1=%p, colors2=%p", 
                     kf1, kf2, colors1, colors2);
            break;
        }
        
        ESP_LOGD(TAG, "Animation frame: keyframe %d->%d, ratio=%.2f, leds=%d", 
                 keyframe_index, keyframe_index + 1, ratio, num_leds);
        
        // Interpolate and apply colors to all LEDs
        for (int i = 0; i < num_leds; i++) {
            led_data_t interpolated;
            interpolate_colors(&colors1[i], &colors2[i], ratio, &interpolated);
            apply_led_color(handle, i, &interpolated);
        }
        
        // Refresh the strip
        led_strip_refresh(handle);
        
        // Control frame rate (30 FPS = 33ms per frame)
        vTaskDelay(pdMS_TO_TICKS(33));
    }
    
    ESP_LOGI(TAG, "Custom animation stopped on strip %d", strip);
    
    // Clean up - free the command copy for this strip
    if (task_params->dynamic_cmd != NULL) {
        ESP_LOGI(TAG, "Freeing command copy for strip %d", strip);
        free(task_params->dynamic_cmd);
        task_params->dynamic_cmd = NULL;
    }
    
    custom_animation_tasks[strip] = NULL;
    vTaskDelete(NULL);
}

// Apply dynamic LED command
static esp_err_t apply_dynamic_command(led_dynamic_command_t* dynamic_cmd) {
    if (!dynamic_cmd) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Applying dynamic LED command, target=%d, keyframes=%d",
             dynamic_cmd->strip_target, dynamic_cmd->keyframe_count);
    
    led_strip_t strips[2];
    int strip_count;
    map_dynamic_target_to_strips(dynamic_cmd->strip_target, strips, &strip_count);
    
    if (strip_count == 0) {
        ESP_LOGE(TAG, "No strips mapped for dynamic target");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate keyframe data
    if (dynamic_cmd->keyframe_count < 2) {
        ESP_LOGE(TAG, "Need at least 2 keyframes for animation");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calculate size needed for a copy of the dynamic command (including keyframes)
    size_t total_size = sizeof(led_dynamic_command_t) + 
                        dynamic_cmd->keyframe_count * sizeof(led_keyframe_t);
    
    ESP_LOGI(TAG, "Allocating %d bytes per strip for command copy (%d keyframes)",
             total_size, dynamic_cmd->keyframe_count);
    
    // Create animation task for each strip (each gets its own copy)
    for (int s = 0; s < strip_count; s++) {
        led_strip_t strip = strips[s];
        
        // Stop any existing animation on this strip
        led_dynamic_stop(strip);
        if (custom_animation_tasks[strip] != NULL) {
            custom_animation_params[strip].stop_requested = true;
            vTaskDelay(pdMS_TO_TICKS(100)); // Wait for task to stop
            
            // Free the old command copy if it exists
            if (custom_animation_params[strip].dynamic_cmd != NULL) {
                free(custom_animation_params[strip].dynamic_cmd);
                custom_animation_params[strip].dynamic_cmd = NULL;
            }
        }
        
        // Create a persistent copy of the command for this strip's animation task
        led_dynamic_command_t* cmd_copy = (led_dynamic_command_t*)malloc(total_size);
        if (!cmd_copy) {
            ESP_LOGE(TAG, "Failed to allocate memory for command copy (strip %d)", strip);
            // Clean up any previous allocations
            for (int i = 0; i < s; i++) {
                if (custom_animation_params[strips[i]].dynamic_cmd != NULL) {
                    free(custom_animation_params[strips[i]].dynamic_cmd);
                    custom_animation_params[strips[i]].dynamic_cmd = NULL;
                }
            }
            return ESP_ERR_NO_MEM;
        }
        
        // Copy the entire structure including keyframes
        memcpy(cmd_copy, dynamic_cmd, total_size);
        ESP_LOGI(TAG, "Command copied successfully for strip %d", strip);
        
        // Setup task parameters with the persistent copy
        custom_animation_params[strip].strip = strip;
        custom_animation_params[strip].dynamic_cmd = cmd_copy;
        custom_animation_params[strip].stop_requested = false;
        
        // Create the animation task with high priority
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "led_anim_%d", strip);
        
        BaseType_t res = xTaskCreatePinnedToCore(
            custom_animation_task,
            task_name,
            8192, // Larger stack for animation calculations
            &custom_animation_params[strip],
            6, // High priority (same as LED manager)
            &custom_animation_tasks[strip],
            0  // CPU0 (same as LED manager)
        );
        
        if (res != pdPASS) {
            ESP_LOGE(TAG, "Failed to create animation task for strip %d", strip);
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "Animation task created for strip %d", strip);
    }
    
    return ESP_OK;
}

// Main function to apply LED commands
esp_err_t led_apply_command(const van_command_t* cmd) {
    if (!cmd) {
        ESP_LOGE(TAG, "NULL command pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (cmd->type != COMMAND_TYPE_LED) {
        ESP_LOGE(TAG, "Not a LED command (type=%d)", cmd->type);
        return ESP_ERR_INVALID_ARG;
    }
    
    const led_command_t* led_cmd = &cmd->command.led_cmd;
    
    ESP_LOGI(TAG, "📡 Applying LED command: type=%s", 
             led_cmd->led_type == LED_STATIC ? "STATIC" : "DYNAMIC");
    
    esp_err_t ret = ESP_OK;
    
    if (led_cmd->led_type == LED_STATIC) {
        ret = apply_static_command(&led_cmd->command.static_cmd);
    } else if (led_cmd->led_type == LED_DYNAMIC) {
        ret = apply_dynamic_command(led_cmd->command.dynamic_cmd);
    } else {
        ESP_LOGE(TAG, "Unknown LED type: %d", led_cmd->led_type);
        ret = ESP_ERR_INVALID_ARG;
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ LED command applied successfully");
    } else {
        ESP_LOGE(TAG, "❌ Failed to apply LED command: %s", esp_err_to_name(ret));
    }
    
    return ret;
}
