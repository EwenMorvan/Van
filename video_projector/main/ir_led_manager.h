#ifndef IR_LED_MANAGER_H
#define IR_LED_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t frequency;     // Fréquence porteuse (Hz)
    uint8_t duty_cycle;     // Rapport cyclique (%)
} ir_config_t;

/**
 * @brief Initialise le gestionnaire LED IR
 * @param pin_ir GPIO de la LED IR (D8)
 * @param config Configuration IR (fréquence, duty cycle)
 * @return 0 si succès, -1 si erreur
 */
int ir_led_manager_init(uint8_t pin_ir, const ir_config_t *config);

/**
 * @brief Envoie une commande IR
 * @param data Données à envoyer
 * @param length Longueur des données
 * @return 0 si succès, -1 si erreur
 * Note: La commande exacte sera déterminée plus tard
 */
int ir_led_manager_send_command(const uint8_t *data, uint16_t length);

/**
 * @brief Active la LED IR (sortie PWM)
 */
void ir_led_manager_enable(void);

/**
 * @brief Désactive la LED IR
 */
void ir_led_manager_disable(void);

/**
 * @brief Récupère l'état de la LED IR
 * @return true si active, false sinon
 */
bool ir_led_manager_is_enabled(void);

#endif // IR_LED_MANAGER_H
