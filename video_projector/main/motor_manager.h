#ifndef MOTOR_MANAGER_H
#define MOTOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MOTOR_DIR_UP = 0,
    MOTOR_DIR_DOWN = 1
} motor_direction_t;

typedef enum {
    MOTOR_STATE_RETRACTED,    // Complètement rétracté (0%)
    MOTOR_STATE_DEPLOYING,    // En cours de déploiement
    MOTOR_STATE_DEPLOYED,     // Complètement déployé (100%)
    MOTOR_STATE_RETRACTING,   // En cours de rétraction
    MOTOR_STATE_STOPPED       // Arrêté en position intermédiaire
} motor_state_t;

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
    motor_state_t state;              // État actuel du moteur
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
 * @return État du moteur (RETRACTED, DEPLOYING, DEPLOYED, RETRACTING, STOPPED)
 */
motor_state_t motor_manager_get_state(void);

/**
 * @brief Vérifie si le vidéoprojecteur est complètement déployé
 * @return true si déployé (100%), false sinon
 */
bool motor_manager_is_deployed(void);

/**
 * @brief Récupère la position actuelle en pourcentage
 * @return Position en % (0.00 à 100.00)
 */
float motor_manager_get_position_percent(void);

/**
 * @brief Récupère la position actuelle
 * @return Position en impulsions encodeur
 */
int32_t motor_manager_get_position(void);

/**
 * @brief Callback appelé quand le moteur change d'état
 * Doit être défini par l'application pour recevoir les notifications
 */
typedef void (*motor_state_callback_t)(motor_state_t new_state, float position_percent);

/**
 * @brief Enregistre un callback pour les changements d'état
 * @param callback Fonction à appeler lors des changements d'état
 */
void motor_manager_set_state_callback(motor_state_callback_t callback);

/**
 * @brief Force un changement d'état
 * @param state Nouveau état
 */
void motor_manager_set_state(motor_state_t state);

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

/**
 * @brief Fait un mouvement JOG sans limites (peut dépasser 0-100%)
 * @param n_turns_tige Nombre de tours de tige
 * @param direction Direction du mouvement
 * @return 0 si succès, -1 si erreur
 */
int motor_manager_jog_unlimited(float n_turns_tige, motor_direction_t direction);

/**
 * @brief Force la position à 100% sans bouger le moteur (calibration haute)
 */
void motor_manager_calibrate_up(void);

/**
 * @brief Force la position à 0% sans bouger le moteur (calibration basse)
 */
void motor_manager_calibrate_down(void);

#endif // MOTOR_MANAGER_H
