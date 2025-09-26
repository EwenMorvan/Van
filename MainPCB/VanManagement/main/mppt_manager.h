#ifndef MPPT_MANAGER_H
#define MPPT_MANAGER_H

#include "esp_err.h"
#include "protocol.h"
#include "log_level.h"

#define MPPT_UPDATE_INTERVAL_MS 2000
#define MPPT_UART_BUFFER_SIZE 512
#define VE_DIRECT_FRAME_SIZE 256

typedef enum {
    MPPT_100_50 = 0,
    MPPT_70_15 = 1
} mppt_id_t;

esp_err_t mppt_manager_init(void);
void mppt_manager_task(void *parameters);

#endif // MPPT_MANAGER_H
