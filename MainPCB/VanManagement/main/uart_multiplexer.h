#ifndef UART_MULTIPLEXER_H
#define UART_MULTIPLEXER_H

#include "esp_err.h"
#include "driver/uart.h"
#include "mppt_manager.h"
#include "log_level.h"

// Use the same MPPT device enum from mppt_manager.h
typedef mppt_id_t mppt_device_t;

// UART multiplexer for sensor devices (UART2)  
typedef enum {
    SENSOR_HEATER = 0,
    SENSOR_HCO2T  = 1
} sensor_device_t;

/**
 * @brief Initialize UART multiplexers
 */
esp_err_t uart_multiplexer_init(void);

/**
 * @brief Switch UART1 to specified MPPT device
 * @param device MPPT device to connect
 * @return ESP_OK on success
 */
esp_err_t uart_mux_switch_mppt(mppt_device_t device);

/**
 * @brief Switch UART2 to specified sensor device  
 * @param device Sensor device to connect
 * @return ESP_OK on success
 */
esp_err_t uart_mux_switch_sensor(sensor_device_t device);

/**
 * @brief Read data from current MPPT device on UART1
 * @param data Buffer to store data
 * @param max_len Maximum length to read
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes read, or -1 on error
 */
int uart_mux_read_mppt(uint8_t *data, size_t max_len, uint32_t timeout_ms);

/**
 * @brief Read data from current sensor device on UART2
 * @param data Buffer to store data
 * @param max_len Maximum length to read  
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes read, or -1 on error
 */
int uart_mux_read_sensor(uint8_t *data, size_t max_len, uint32_t timeout_ms);

#endif // UART_MULTIPLEXER_H
