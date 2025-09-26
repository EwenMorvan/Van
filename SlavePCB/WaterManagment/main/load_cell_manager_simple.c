#include "esp_log.h"
#include "driver/gpio.h"
#include "gpio_pinout.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <stdbool.h>
#include "slave_pcb.h"

#define NUM_LOAD_CELLS 5

static const char *TAG = "LoadCellManager";

void hx711_init(void) {
    gpio_set_direction(HX_711_SCK, GPIO_MODE_OUTPUT);
    uint32_t data_pins[] = {HX_711_DT_A, HX_711_DT_B, HX_711_DT_C, HX_711_DT_D, HX_711_DT_E};
    for (int i = 0; i < NUM_LOAD_CELLS; i++) {
        gpio_set_direction(data_pins[i], GPIO_MODE_INPUT);
    }
}

uint32_t hx711_read(uint32_t data_pin) {
    uint32_t value = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(HX_711_SCK, 1);
        esp_rom_delay_us(1);
        value = (value << 1) | gpio_get_level(data_pin);
        gpio_set_level(HX_711_SCK, 0);
        esp_rom_delay_us(1);
    }
    gpio_set_level(HX_711_SCK, 1);
    esp_rom_delay_us(1);
    gpio_set_level(HX_711_SCK, 0);
    return value;
}

void load_cell_manager_task(void *pvParameters) {
    uint32_t data_pins[] = {HX_711_DT_A, HX_711_DT_B, HX_711_DT_C, HX_711_DT_D, HX_711_DT_E};
    while (true) {
        for (int i = 0; i < NUM_LOAD_CELLS; i++) {
            uint32_t raw_value = hx711_read(data_pins[i]);
            ESP_LOGI(TAG, "Load Cell %d Raw Value: %u", i + 1, raw_value);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

slave_pcb_err_t load_cell_manager_init(void) {
    ESP_LOGI(TAG, "Initializing HX711 Load Cells...");
    hx711_init();
    return SLAVE_PCB_OK;
}