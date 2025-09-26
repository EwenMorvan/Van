#ifndef FAN_MANAGER_H
#define FAN_MANAGER_H

#include "esp_err.h"
#include "log_level.h"

#define FAN_PWM_FREQUENCY 25000  // 25 kHz PWM frequency
#define FAN_PWM_RESOLUTION 8     // 8-bit resolution (0-255)

typedef enum {
    FAN_ELEC_BOX = 0,
    FAN_HEATER = 1,
    FAN_HOOD = 2
} fan_id_t;

esp_err_t fan_manager_init(void);
void fan_manager_task(void *parameters);
esp_err_t fan_set_speed(fan_id_t fan, uint8_t speed_percent);
esp_err_t fan_start(fan_id_t fan);
esp_err_t fan_stop(fan_id_t fan);
uint8_t fan_get_speed(fan_id_t fan);
esp_err_t fan_manager_set_hood_state(bool enabled);

#endif // FAN_MANAGER_H
