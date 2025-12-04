#ifndef APP_MAIN_COMMUNICATION_MANAGER_H
#define APP_MAIN_COMMUNICATION_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"

#include "protocol.h"
#include "../utils/battery_parser.h"
#include "../utils/json_builder.h"
#include "../communications/ble/ble_manager_nimble.h"


esp_err_t app_main_communication_manager_init(void);

esp_err_t app_main_send_van_state_to_app(void);

#endif // APP_MAIN_COMMUNICATION_MANAGER_H