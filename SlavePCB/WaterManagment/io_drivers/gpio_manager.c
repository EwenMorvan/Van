#include "gpio_manager.h"

static const char *TAG = "GPIO_MGR";
/**
 * @brief Initialize GPIO pins
 * @return slave_pcb_err_t Error code
 */
slave_pcb_err_t init_gpio(void) {
    ESP_LOGI(TAG, "Initializing GPIO");

    // Configure input pins for buttons
    gpio_config_t input_config = {
        .pin_bit_mask = (1ULL << BE1) | (1ULL << BE2) | (1ULL << BD1) | 
                       (1ULL << BD2) | (1ULL << BH),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,// Already pulled down by PCB
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&input_config);
    if (ret != ESP_OK) {
        REPORT_ERROR(SLAVE_PCB_ERR_INIT_FAIL, TAG, "Failed to configure input GPIO", ret);
        return SLAVE_PCB_ERR_INIT_FAIL;
    }

    // Configure output pins for shift registers and control signals
    gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << REG_MR) | (1ULL << REG_DS) | (1ULL << REG_STCP) |
                       (1ULL << REG_SHCP) | (1ULL << REG_OE) | (1ULL << HX_711_SCK) |
                       (1ULL << W5500_RST) | (1ULL << I2C_MUX_A0) | (1ULL << I2C_MUX_A1) |
                       (1ULL << I2C_MUX_A2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&output_config);
    if (ret != ESP_OK) {
        REPORT_ERROR(SLAVE_PCB_ERR_INIT_FAIL, TAG, "Failed to configure output GPIO", ret);
        return SLAVE_PCB_ERR_INIT_FAIL;
    }

    // Configure HX711 DT pins as inputs
    gpio_config_t hx711_config = {
        .pin_bit_mask = (1ULL << HX_711_DT_A) | (1ULL << HX_711_DT_B) | 
                       (1ULL << HX_711_DT_C) | (1ULL << HX_711_DT_D) | 
                       (1ULL << HX_711_DT_E),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&hx711_config);
    if (ret != ESP_OK) {
        REPORT_ERROR(SLAVE_PCB_ERR_INIT_FAIL, TAG, "Failed to configure HX711 GPIO", ret);
        return SLAVE_PCB_ERR_INIT_FAIL;
    }

    ESP_LOGI(TAG, "GPIO initialization completed");
    return SLAVE_PCB_OK;
}
