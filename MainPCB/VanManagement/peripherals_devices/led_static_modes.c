#include "led_static_modes.h"
#include "led_manager.h" 
#include "gpio_pinout.h"
#include "esp_log.h"
#include "led_strip.h"
#include "led_strip_spi.h"  // Use SPI backend for roof1 strips
#include "driver/rmt_tx.h"  // Use RMT for roof2 and exterior strips

static const char *TAG = "LED_STATIC";

// Static flag to track if SPI bus is initialized
static bool spi3_bus_initialized = false;

esp_err_t led_static_init_strips(led_strip_handle_t strips[])
{
    esp_err_t ret;

    // ROOF STRIPS - HYBRID APPROACH
    // Strip 2 uses RMT with DMA (Avoid ble interruption interference)
    // Strip 1 uses SPI3 (Avoid ble interruption interference with SPI3 since no GDMA channel left)

    // LED Strip 1 (Roof) - 120 LEDs on SPI3
    led_strip_config_t strip_config_1 = {
        .strip_gpio_num = DI_LED1,
        .max_leds = LED_STRIP_1_COUNT,
        .led_model = LED_MODEL_SK6812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRBW,
        .flags.invert_out = false
    };
    
    led_strip_spi_config_t spi_config_1 = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = SPI3_HOST,
        .flags.with_dma = true,
    };

    ret = led_strip_new_spi_device(&strip_config_1, &spi_config_1, &strips[LED_ROOF_STRIP_1]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip 1: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "LED Strip 1 (Roof) initialized on GPIO %d via SPI3", DI_LED1);

    // LED Strip 2 (Roof) - 120 LEDs on RMT with DMA
    // Using RMT with minimal memory to use only 1 RMT TX channel
    led_strip_config_t strip_config_2 = {
        .strip_gpio_num = DI_LED2,
        .max_leds = LED_STRIP_2_COUNT,
        .led_model = LED_MODEL_SK6812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRBW,
        .flags.invert_out = false
    };
    
    led_strip_rmt_config_t rmt_config_2 = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = true,  // DMA
        .mem_block_symbols = 48 
    };

    ret = led_strip_new_rmt_device(&strip_config_2, &rmt_config_2, &strips[LED_ROOF_STRIP_2]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip 2: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "LED Strip 2 (Roof) initialized on GPIO %d via RMT+DMA", DI_LED2);

    // EXTERIOR STRIPS USE RMT
    // Only 1 RMT channel is free, for now we only initialize EXT_FRONT since tehre is no led connected to EXT_BACK
    // Due to the BLE interuptions, and since there is no GDMA or SPI channel left, the refresh of the strip may be affected
    // So Exterior strips should use only simple static modes.
    // Note: if EXT_BACK is needed in the future, found a solution to multiplex RMT channels or use non adressable LEDs. 
    
    // LED Strip EXT_FRONT - 60 LEDs on RMT
    led_strip_config_t strip_config_ext_front = {
        .strip_gpio_num = DI_LED_AV,
        .max_leds = LED_STRIP_EXT_FRONT_COUNT,
        .led_model = LED_MODEL_SK6812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRBW,
        .flags.invert_out = false
    };
    
    led_strip_rmt_config_t rmt_config_ext_front = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,  // 60 LEDs work fine without DMA
        .mem_block_symbols = 64
    };

    ret = led_strip_new_rmt_device(&strip_config_ext_front, &rmt_config_ext_front, &strips[LED_EXT_FRONT]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip EXT_FRONT: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "LED Strip EXT_FRONT initialized on GPIO %d via RMT", DI_LED_AV);

    // // LED Strip EXT_BACK - 60 LEDs on RMT
    // led_strip_config_t strip_config_ext_back = {
    //     .strip_gpio_num = DI_LED_AR,
    //     .max_leds = LED_STRIP_EXT_BACK_COUNT,
    //     .led_model = LED_MODEL_SK6812,
    //     .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRBW,
    //     .flags.invert_out = false
    // };
    
    // led_strip_rmt_config_t rmt_config_ext_back = {
    //     .clk_src = RMT_CLK_SRC_DEFAULT,
    //     .resolution_hz = 10 * 1000 * 1000,
    //     .flags.with_dma = false,  // 60 LEDs work fine without DMA
    //     .mem_block_symbols = 64
    // };

    // ret = led_strip_new_rmt_device(&strip_config_ext_back, &rmt_config_ext_back, &strips[LED_EXT_BACK]);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to create LED strip EXT_BACK: %s", esp_err_to_name(ret));
    //     return ret;
    // }
    // ESP_LOGI(TAG, "LED Strip EXT_BACK initialized on GPIO %d via RMT", DI_LED_AR);

    return ESP_OK;
}

// Generic helper to fill a strip with color
static void set_strip_color(led_strip_handle_t handle, int num_leds,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t brightness)
{
    if (!handle || num_leds <= 0) return;

    // Scale RGBW values by brightness (0–255)
    float scale = brightness / 255.0f;

    for (int i = 0; i < num_leds; i++)
    {
        led_strip_set_pixel_rgbw(handle,
                                 i,
                                 (uint8_t)(r * scale),
                                 (uint8_t)(g * scale),
                                 (uint8_t)(b * scale),
                                 (uint8_t)(w * scale));
    }

    led_strip_refresh(handle);
}

// --- Public static modes ---
void led_static_off(led_strip_t strip, uint8_t brightness)
{
    led_strip_handle_t handle = led_manager_get_handle(strip);
    int num_leds = led_manager_get_led_count(strip);
    set_strip_color(handle, num_leds, 0, 0, 0, 0, brightness);
}

void led_static_white(led_strip_t strip, uint8_t brightness)
{
    led_strip_handle_t handle = led_manager_get_handle(strip);
    int num_leds = led_manager_get_led_count(strip);
    set_strip_color(handle, num_leds, 0, 0, 0, 255, brightness);
}

void led_static_orange(led_strip_t strip, uint8_t brightness)
{
    led_strip_handle_t handle = led_manager_get_handle(strip);
    int num_leds = led_manager_get_led_count(strip);
    set_strip_color(handle, num_leds, 220, 120, 0, 0, brightness);
}

void led_static_film(led_strip_t strip, uint8_t brightness)
{
    led_strip_handle_t handle = led_manager_get_handle(strip);
    int num_leds = led_manager_get_led_count(strip);
    set_strip_color(handle, num_leds, 30, 10, 0, 0, brightness);
}
