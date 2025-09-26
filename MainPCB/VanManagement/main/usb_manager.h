#ifndef USB_MANAGER_H
#define USB_MANAGER_H

#include "esp_err.h"
#include "protocol.h"
#include "log_level.h"

esp_err_t usb_manager_init(void);
void usb_manager_task(void *parameters);
esp_err_t usb_send_state(van_state_t *state);

#endif // USB_MANAGER_H
