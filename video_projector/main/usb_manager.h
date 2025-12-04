#ifndef USB_MANAGER_H
#define USB_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*usb_power_callback_t)(bool is_powered);

/**
 * @brief Initialise le gestionnaire USB
 * @param pin_usb_flag GPIO pour la détection d'alimentation USB (D1)
 * @param callback Fonction appelée lors d'un changement d'état d'alimentation
 * @return 0 si succès, -1 si erreur
 */
int usb_manager_init(uint8_t pin_usb_flag, usb_power_callback_t callback);

/**
 * @brief Vérifie si l'alimentation USB est présente
 * @return true si USB alimenté, false sinon
 */
bool usb_manager_is_powered(void);

/**
 * @brief Récupère l'état du drapeau USB
 * @return État GPIO du drapeau USB
 */
bool usb_manager_get_flag(void);

#endif // USB_MANAGER_H
