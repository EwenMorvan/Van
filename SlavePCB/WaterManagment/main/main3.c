#include "../common_includes/utils.h"
#include "../common_includes/error_manager.h"
#include "../io_drivers/shift_register.h"
#include "../io_drivers/gpio_manager.h"
#include "../communications/uart/uart_manager.h"
#include "../communications/communications_manager.h"
#include "cases_manager.h"

static const char *TAG = "SLAVE_PCB_MAIN";


static slave_pcb_err_t init_components(void){
    ESP_LOGI(TAG, "Initializing components...");
    slave_pcb_err_t ret;

    // Initialize error manager
    error_manager_init();
    
    // Initialize GPIO
    ret = init_gpio();
    if (ret != SLAVE_PCB_OK) {
        REPORT_ERROR(ret, TAG, "GPIO initialization failed", 0);
        return SLAVE_PCB_ERR_INIT_FAIL;
    }

    // Initialize shift registers
    ret = init_shift_registers();
    if (ret != SLAVE_PCB_OK) {
        REPORT_ERROR(ret, TAG, "Failed to initialize shift registers", 0);
        return SLAVE_PCB_ERR_INIT_FAIL;
    }

    // Initialize UART manager
    ret = uart_manager_init();
    if (ret != SLAVE_PCB_OK) {
        REPORT_ERROR(ret, TAG, "Failed to initialize UART Manager", 0);
        return SLAVE_PCB_ERR_INIT_FAIL;
    }

    // Initialize case manager
    ret = cases_manager_init();
    if (ret != SLAVE_PCB_OK) {
        REPORT_ERROR(ret, TAG, "Failed to initialize Cases Manager", 0);
        return SLAVE_PCB_ERR_INIT_FAIL;
    }

    // Initialize communication manager
    ret = communications_manager_init();
    if (ret != SLAVE_PCB_OK) {
        REPORT_ERROR(ret, TAG, "Failed to initialize Communication Manager", 0);
        return SLAVE_PCB_ERR_INIT_FAIL;
    }

    // Test a error reporting of each error type

    // REPORT_ERROR(SLAVE_PCB_ERR_INVALID_ARG, TAG, "Test Initialization error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_INIT_FAIL, TAG, "Test Initialization error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_MEMORY, TAG, "Test Initialization error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_COMM_FAIL, TAG, "Test Communication error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_I2C_FAIL, TAG, "Test Communication error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_SPI_FAIL, TAG, "Test Communication error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_TIMEOUT, TAG, "Test Communication error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_ETH_DISCONNECTED, TAG, "Test Communication error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_DEVICE_NOT_FOUND, TAG, "Test Device error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_DEVICE_BUSY, TAG, "Test Device error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_DEVICE_FAULT, TAG, "Test Device error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_STATE_INVALID, TAG, "Test State/Case error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_INCOMPATIBLE_CASE, TAG, "Test State/Case error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_CASE_TRANSITION, TAG, "Test State/Case error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_SAFETY_LIMIT, TAG, "Test Safety error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_EMERGENCY_STOP, TAG, "Test Safety error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_OVERCURRENT, TAG, "Test Safety error", 0);
    // REPORT_ERROR(SLAVE_PCB_ERR_SENSOR_RANGE, TAG, "Test Safety error", 0);


    ESP_LOGI(TAG, "Components initialized successfully!");
    return SLAVE_PCB_OK;

}

void app_main(void) {
    ESP_LOGI(TAG, "SlavePCB starting up...");
    slave_pcb_err_t ret;

    // Initialize all components
    ret = init_components();
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to initialize components: %s", get_error_string(ret));
        REPORT_ERROR(ret, TAG, "Failed to initialize components", 0);
        return;
    }

    // Apply initial case logic (RST)
    ret = apply_case_logic(CASE_RST);
    if (ret != SLAVE_PCB_OK) {
        ESP_LOGE(TAG, "Failed to apply initial case logic: %s", get_error_string(ret));
        REPORT_ERROR(ret, TAG, "Failed to apply initial case logic", 0);
        return;
    }



    ESP_LOGI(TAG, "SlavePCB completely initialized!");
    // while(1) {
    //     vTaskDelay(pdMS_TO_TICKS(10000));

    // }
}


