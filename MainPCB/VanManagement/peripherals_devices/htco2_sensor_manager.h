#ifndef HTCO2_SENSOR_MANAGER_H
#define HTCO2_SENSOR_MANAGER_H

#include "esp_err.h"

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
 * @brief Optional: update van_state snapshot (not required if task is running)
 */
esp_err_t htco2_sensor_manager_update_van_state(void);

#endif // HTCO2_SENSOR_MANAGER_H
