#ifndef MPPT_MANAGER_H
#define MPPT_MANAGER_H

#include "esp_err.h"
#include "../communications/protocol.h"

#define MPPT_UPDATE_INTERVAL_MS 2000
#define MPPT_UART_BUFFER_SIZE 512
#define VE_DIRECT_FRAME_SIZE 256

typedef enum {
    MPPT_100_50 = 0,
    MPPT_70_15 = 1
} mppt_id_t;

esp_err_t mppt_manager_init(void);
void mppt_manager_task(void *parameters);

/**
 * @brief Update van_state with MPPT data (simulated or real)
 * @param van_state Pointer to van_state structure to update
 * @return ESP_OK on success
 */
esp_err_t mppt_manager_update_van_state(van_state_t* van_state);

#endif // MPPT_MANAGER_H
