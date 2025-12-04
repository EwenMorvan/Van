#ifndef MOTOR_MANAGER_H
#define MOTOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MOTOR_DIR_UP = 0,
    MOTOR_DIR_DOWN = 1
} motor_direction_t;

typedef struct {
    // Configuration des broches
    uint8_t pin_sleep;      // D5
    uint8_t pin_dir1;       // D6
    uint8_t pin_dir2;       // D7
    uint8_t pin_enc_a;      // D2
    uint8_t pin_enc_b;      // D3
    
    // Paramètres de la tige filetée
    float turns_per_complete_travel;  // Tours encodeur pour course complète
    float gear_ratio;                 // Ratio réduction moteur/tige
    
    // État
    int32_t current_position;         // Position actuelle en impulsions encodeur
    int32_t target_position;          // Position cible
    bool is_deployed;                 // État du vidéoprojecteur
} motor_config_t;

/**
 * @brief Initialise le gestionnaire moteur
 * @param config Configuration du moteur
 * @return 0 si succès, -1 si erreur
 */
int motor_manager_init(const motor_config_t *config);

/**
 * @brief Déploie le vidéoprojecteur
 * @return 0 si succès, -1 si erreur
 */
int motor_manager_deploy_video_proj(void);

/**
 * @brief Rétracte le vidéoprojecteur
 * @return 0 si succès, -1 si erreur
 */
int motor_manager_retract_video_proj(void);

/**
 * @brief Fait tourner le moteur d'un certain nombre de tours
 * @param n_turns Nombre de tours (peut être négatif)
 * @param direction Direction de rotation
 * @return 0 si succès, -1 si erreur
 */
int motor_manager_turn(float n_turns, motor_direction_t direction);

/**
 * @brief Vérifie si la cible a été atteinte et arrête le moteur
 * Doit être appelée régulièrement depuis une tâche de monitoring
 */
void motor_manager_check_target(void);

/**
 * @brief Récupère l'état actuel du moteur
 * @return true si déployé, false sinon
 */
bool motor_manager_is_deployed(void);

/**
 * @brief Récupère la position actuelle
 * @return Position en impulsions encodeur
 */
int32_t motor_manager_get_position(void);

/**
 * @brief Arrête le moteur immédiatement
 */
void motor_manager_stop(void);

/**
 * @brief Contrôle le moteur avec PWM direct
 * @param pwm Valeur PWM signée (-255 à +255)
 *            Positif = direction UP, Négatif = direction DOWN, 0 = arrêt
 */
void motor_manager_set_pwm(int16_t pwm);

/**
 * @brief Fait un mouvement de réglage fin (jog) de la tige
 * @param n_turns_tige Nombre de tours de tige (1.0, 0.1, 0.01)
 * @param direction Direction du mouvement (MOTOR_DIR_UP ou MOTOR_DIR_DOWN)
 * @return 0 si succès, -1 si erreur
 */
int motor_manager_jog(float n_turns_tige, motor_direction_t direction);

#endif // MOTOR_MANAGER_H
