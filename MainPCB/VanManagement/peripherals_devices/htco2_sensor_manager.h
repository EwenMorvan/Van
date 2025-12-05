#ifndef HTCO2_SENSOR_MANAGER_H
#define HTCO2_SENSOR_MANAGER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include <inttypes.h>

#include "../communications/protocol.h"
#include "../communications/uart/uart_multiplexer.h"
#include "../common_includes/gpio_pinout.h"

typedef struct{
    uint32_t co2;
    int32_t t_tenths;
    int32_t h_tenths;
    int32_t light;
}htco2_sensor_t;

/**
 * @brief Initialize HCO2T sensor manager.
 *
 * Starts a background task that periodically (every 5s) reads the HCO2T sensor
 * via the UART multiplexer and updates `protocol` van state sensor fields.
 */
esp_err_t htco2_sensor_manager_init(void);

/**
 * @brief Deinitialize HCO2T sensor manager.
 */
esp_err_t htco2_sensor_manager_deinit(void);

/**
 * @brief Update van_state
 */
esp_err_t htco2_sensor_manager_update_van_state(van_state_t* van_state);

#endif // HTCO2_SENSOR_MANAGER_H
