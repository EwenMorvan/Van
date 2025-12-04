#include "led_dynamic_modes.h"
#include "led_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_DYNAMIC";

// Task handles array to track running animations
static TaskHandle_t animation_tasks[LED_STRIP_COUNT] = {NULL};

///////////////////////////////////// Rainbow Mode /////////////////////////////////////
static void color_wheel(uint8_t pos, uint8_t brightness,
                        uint8_t *r, uint8_t *g, uint8_t *b)
{
    pos = 255 - pos;
    if (pos < 85)
    {
        *r = (255 - pos * 3) * brightness / 255;
        *g = 0;
        *b = (pos * 3) * brightness / 255;
    }
    else if (pos < 170)
    {
        pos -= 85;
        *r = 0;
        *g = (pos * 3) * brightness / 255;
        *b = (255 - pos * 3) * brightness / 255;
    }
    else
    {
        pos -= 170;
        *r = (pos * 3) * brightness / 255;
        *g = (255 - pos * 3) * brightness / 255;
        *b = 0;
    }
}

// Rainbow animation task
typedef struct
{
    led_strip_t strip;   // enum index
    uint16_t offset;
    uint8_t brightness;
    volatile bool stop_requested;
} rainbow_task_t;

static rainbow_task_t rainbow_tasks[LED_STRIP_COUNT];

// Main animation function
static void rainbow_animation_task(void *param)
{
    rainbow_task_t *rt = (rainbow_task_t *)param;
    if (!rt) vTaskDelete(NULL);

    led_strip_handle_t handle = led_manager_get_handle(rt->strip);
    int num_leds = led_manager_get_led_count(rt->strip);

    if (!handle || num_leds <= 0)
    {
        ESP_LOGE(TAG, "Invalid LED handle or count for strip %d", rt->strip);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Starting rainbow animation on strip %d (%d LEDs)", rt->strip, num_leds);

    while (1)
    {
        for (int i = 0; i < num_leds; i++)
        {
            uint8_t wheel_pos = (i * 256 / num_leds + rt->offset) & 0xFF;
            uint8_t r, g, b;
            color_wheel(wheel_pos, rt->brightness, &r, &g, &b);
            led_strip_set_pixel_rgbw(handle, i, r, g, b, 0);
        }

        led_strip_refresh(handle);
        rt->offset = (rt->offset + 1) % 256;
        vTaskDelay(pdMS_TO_TICKS(50));  // ~20 FPS
        if (rt->stop_requested) {
            animation_tasks[rt->strip] = NULL;
            vTaskDelete(NULL);
        }
    }
}

// Task handles array to track running animations
// static TaskHandle_t animation_tasks[LED_STRIP_COUNT] = {NULL}; // Moved to top
////////////////////////////////////////////////////////////////////////////////

////////////////////////// Door open animation ///////////////////////////////////
typedef struct
{
    led_strip_t strip;
    uint8_t brightness;
    bool direction;
    volatile bool stop_requested;
} door_open_task_t;

static door_open_task_t door_open_tasks[LED_STRIP_COUNT];

static void door_open_animation_task(void *param)
{
    door_open_task_t *dt = (door_open_task_t *)param;
    if (!dt) vTaskDelete(NULL);

    led_strip_handle_t handle = led_manager_get_handle(dt->strip);
    int num_leds = led_manager_get_led_count(dt->strip);

    if (!handle || num_leds <= 0)
    {
        ESP_LOGE(TAG, "Invalid LED handle or count for strip %d", dt->strip);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Starting door open animation on strip %d (%d LEDs)", dt->strip, num_leds);

    // Define colors
    uint8_t sunset_r = 255;
    uint8_t sunset_g = 100;
    uint8_t sunset_b = 0;
    uint8_t sunset_w = 0;
    uint8_t white_r = 0;
    uint8_t white_g = 0;
    uint8_t white_b = 0;
    uint8_t white_w = 255;



    // Check direction: 1 for intro, 0 for outro
    if (dt->direction == 1)
    {
        int total_time_ms = 5000; // Total time for intro is 5 seconds
        int delay_per_step = total_time_ms / num_leds;
        if (delay_per_step < 10) delay_per_step = 10; // Minimum delay

    
        // Intro: wake up wave simulating sunrise, gradient from sunrise to white over 20 LEDs with brightness variation
        for (int pos = num_leds - 1; pos >= 0; pos--)
        {
            for (int i = 0; i < num_leds; i++)
            {
                if (i >= pos - 19 && i <= pos)
                {
                    int dist = pos - i; // 0 at front (pos), 19 at back (pos-19)
                    float t = 1.0f - (float)dist / 19.0f; // Invert: 1 at front, 0 at back
                    // Color interpolation from sunset to white
                    float r_base = sunset_r * (1 - t) + white_r * t;
                    float g_base = sunset_g * (1 - t) + white_g * t;
                    float b_base = sunset_b * (1 - t) + white_b * t;
                    float w_base = sunset_w * (1 - t) + white_w * t;
                    // Brightness variation: lower at back, higher at front
                    float brightness_factor = 0.3f + 0.7f * t;
                    uint8_t local_bright = (uint8_t)(dt->brightness * brightness_factor);
                    uint8_t r = (uint8_t)(r_base * local_bright / 255);
                    uint8_t g = (uint8_t)(g_base * local_bright / 255);
                    uint8_t b = (uint8_t)(b_base * local_bright / 255);
                    uint8_t w = (uint8_t)(w_base * local_bright / 255);
                    led_strip_set_pixel_rgbw(handle, i, r, g, b, w);
                }
                else if (i > pos)
                {
                    // Ahead: set to white
                    led_strip_set_pixel_rgbw(handle, i, white_r, white_g, white_b, (uint8_t)(white_w * dt->brightness / 255));
                }
                // Behind: keep as is
            }
            if (handle) led_strip_refresh(handle);
            vTaskDelay(pdMS_TO_TICKS(delay_per_step));
            if (dt->stop_requested) {
                animation_tasks[dt->strip] = NULL;
                vTaskDelete(NULL);
            }
        }
        // After intro, all LEDs are white, stay that way until outro is called
    }
    else
    {
        // Outro: Reverse sunrise wave, starting from front to back, fading to sunset colors
        // Ensure all LEDs are white before starting outro
        for (int i = 0; i < num_leds; i++)
        {
            led_strip_set_pixel_rgbw(handle, i, white_r, white_g, white_b, white_w);
        }
        if (handle) led_strip_refresh(handle);

        int total_time_ms = 60000; // Total time for outro is 1 min
        int delay_per_step = total_time_ms / num_leds;
        if (delay_per_step < 10) delay_per_step = 10; // Minimum delay

        // Outro: wave from front (pos=0) to back (pos=19), turning off with white to sunset gradient
        for (int pos = 0; pos < num_leds; pos++)
        {
            for (int i = 0; i < num_leds; i++)
            {
                if (i >= pos && i <= pos + 19)
                {
                    int dist = i - pos; // 0 at front (pos), 19 at back (pos+19)
                    float t = (float)dist / 19.0f; // 0 at front, 1 at back
                    // Color interpolation from sunset (red) to white
                    float r_base = sunset_r * (1 - t) + white_r * t;
                    float g_base = sunset_g * (1 - t) + white_g * t;
                    float b_base = sunset_b * (1 - t) + white_b * t;
                    float w_base = sunset_w * (1 - t) + white_w * t;
                    // Brightness variation: from off (at front) to full (at back)
                    float brightness_factor = t;
                    uint8_t local_bright = (uint8_t)(dt->brightness * brightness_factor);
                    uint8_t r = (uint8_t)(r_base * local_bright / 255);
                    uint8_t g = (uint8_t)(g_base * local_bright / 255);
                    uint8_t b = (uint8_t)(b_base * local_bright / 255);
                    uint8_t w = (uint8_t)(w_base * local_bright / 255);
                    led_strip_set_pixel_rgbw(handle, i, r, g, b, w);
                }
                else if (i < pos)
                {
                    // Behind: off
                    led_strip_set_pixel_rgbw(handle, i, 0, 0, 0, 0);
                }
                // Ahead: keep white
            }
            if (handle) led_strip_refresh(handle);
            vTaskDelay(pdMS_TO_TICKS(delay_per_step));
            if (dt->stop_requested) {
                animation_tasks[dt->strip] = NULL;
                vTaskDelete(NULL);
            }
        }
        // Ensure all LEDs are off at the end
        for (int i = 0; i < num_leds; i++)
        {
            led_strip_set_pixel_rgbw(handle, i, 0, 0, 0, 0);
        }
        if (handle) led_strip_refresh(handle);
        // Set mode to OFF after outro for clean state
        led_set_mode(dt->strip, LED_MODE_OFF);
    }    // Animation complete, task ends
    animation_tasks[dt->strip] = NULL;
    vTaskDelete(NULL);
}

//////////////////////////////////////////////////////////////////////////////////
// Public API
esp_err_t led_dynamic_rainbow(led_strip_t strip, uint8_t brightness)
{
    ESP_LOGI(TAG, "Starting rainbow mode on strip %d with brightness %d", strip, brightness);

    if (strip >= LED_STRIP_COUNT)
        return ESP_ERR_INVALID_ARG;

    // Stop any existing animation on this strip
    led_dynamic_stop(strip);

    // Initialize task parameters
    rainbow_tasks[strip].strip = strip;
    rainbow_tasks[strip].offset = 0;
    rainbow_tasks[strip].brightness = brightness;
    rainbow_tasks[strip].stop_requested = false;

    // Create the animation task with larger stack
    BaseType_t res = xTaskCreate(rainbow_animation_task, "led_rainbow",
                                 4096,  // Increased stack size
                                 &rainbow_tasks[strip],
                                 5,
                                 &animation_tasks[strip]);

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create rainbow animation task");
        animation_tasks[strip] = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Rainbow animation task created successfully");
    return ESP_OK;
}

esp_err_t led_dynamic_door_open(led_strip_t strip, uint8_t brightness, bool direction)
{
    ESP_LOGI(TAG, "Starting door open animation on strip %d with brightness %d", strip, brightness);

    if (strip >= LED_STRIP_COUNT)
        return ESP_ERR_INVALID_ARG;

    // Stop any existing animation on this strip
    led_dynamic_stop(strip);

    // Initialize task parameters
    door_open_tasks[strip].strip = strip;
    door_open_tasks[strip].brightness = brightness;
    door_open_tasks[strip].direction = direction;
    door_open_tasks[strip].stop_requested = false;

    // Create the animation task
    BaseType_t res = xTaskCreate(door_open_animation_task, "led_door_open",
                                 4096,  // Stack size
                                 &door_open_tasks[strip],
                                 5,
                                 &animation_tasks[strip]);

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create door open animation task");
        animation_tasks[strip] = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Door open animation task created successfully");
    return ESP_OK;
}

void led_dynamic_stop(led_strip_t strip)
{
    if (strip >= LED_STRIP_COUNT) return;

    // Set stop flags for both types
    rainbow_tasks[strip].stop_requested = true;
    door_open_tasks[strip].stop_requested = true;

    // The tasks will check the flag and delete themselves
    ESP_LOGI(TAG, "Stopping animation on strip %d", strip);
}
