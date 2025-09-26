#ifndef WIFI_OTA_MANAGER_H
#define WIFI_OTA_MANAGER_H

#include "slave_pcb.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WiFi and OTA manager
 * 
 * This function initializes WiFi connection and starts an HTTP server
 * for Over-The-Air firmware updates.
 * 
 * @return slave_pcb_err_t Success or error code
 */
slave_pcb_err_t wifi_ota_init(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected, false otherwise
 */
bool wifi_ota_is_connected(void);

/**
 * @brief Stop WiFi and OTA services
 */
void wifi_ota_stop(void);

/**
 * @brief Add a log message to the web log buffer
 * 
 * @param message Log message to add
 */
void add_web_log(const char* message);

#ifdef __cplusplus
}
#endif

#endif // WIFI_OTA_MANAGER_H
