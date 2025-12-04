#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BUTTON_EVENT_SHORT_PRESS,
    BUTTON_EVENT_LONG_PRESS,
    BUTTON_EVENT_RELEASED
} button_event_t;

typedef void (*button_callback_t)(button_event_t event);

/**
 * @brief Initialise le gestionnaire bouton
 * @param pin_button GPIO du bouton (D9)
 * @param pin_led GPIO de la LED (D10)
 * @param callback Fonction appelée lors d'un événement bouton
 * @return 0 si succès, -1 si erreur
 */
int button_manager_init(uint8_t pin_button, uint8_t pin_led, button_callback_t callback);

/**
 * @brief Active/désactive la LED du bouton
 * @param enabled true pour activer, false pour désactiver
 */
void button_manager_set_led(bool enabled);

/**
 * @brief Récupère l'état de la LED
 * @return true si allumée, false sinon
 */
bool button_manager_get_led_state(void);

/**
 * @brief Fonction appelée par l'ISR GPIO
 * (Usage interne)
 */
void button_manager_isr_handler(void);

#endif // BUTTON_MANAGER_H
