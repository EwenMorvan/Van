#include "motor_manager.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "hal/gpio_ll.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "MOTOR_MANAGER";

static motor_config_t g_motor_config;
static volatile int32_t g_encoder_count = 0;
static bool g_is_moving = false;

// Variables pour le suivi des mouvements
static int32_t g_move_target = 0;
static motor_direction_t g_move_direction = MOTOR_DIR_UP;

// PWM et accélération (2 phases: ACCEL + CRUISE)
static volatile uint8_t g_pwm_duty = 0;           // 0-255 (duty cycle)
static const uint8_t PWM_MAX_DUTY = 255;          // PWM max (255 = 100% duty cycle)
static const uint8_t PWM_ACCEL_STEP = 5;          // +5 duty tous les 10ms → ~0.5s pour 0→255

// Phases de mouvement
typedef enum {
    PHASE_ACCEL,        // Phase 1: accélération (0→255)
    PHASE_CRUISE,       // Phase 2: vitesse max constante (255)
    PHASE_DECEL         // Phase 3: décélération progressive (255→PWM_MIN)
} motor_phase_t;

static volatile motor_phase_t g_motor_phase = PHASE_ACCEL;

// ISR encodeur - Variables volatiles
static volatile uint8_t g_last_state = 0;
static volatile int64_t g_last_pulse_time = 0;
static portMUX_TYPE g_encoder_spinlock = portMUX_INITIALIZER_UNLOCKED;

// Table de décodage quadrature optimisée pour ISR
// Index: (old_state << 2) | new_state
// Sens normal (UP = positif, DOWN = négatif)
static const int8_t IRAM_ATTR g_quadrature_lookup[16] = {
     0,  // 00 00
    -1,  // 00 01 (0→1)
     1,  // 00 10 (0→2)
     0,  // 00 11
     1,  // 01 00 (1→0)
     0,  // 01 01
     0,  // 01 10
    -1,  // 01 11 (1→3)
    -1,  // 10 00 (2→0)
     0,  // 10 01
     0,  // 10 10
     1,  // 10 11 (2→3)
     0,  // 11 00
     1,  // 11 01 (3→1)
    -1,  // 11 10 (3→2)
     0   // 11 11
};

/**
 * @brief ISR encodeur - Décodage quadrature en temps réel
 * Déclenché sur chaque transition GPIO (ANYEDGE)
 * Utilise lookup table pour décodage O(1) ultra-rapide
 */
static void IRAM_ATTR encoder_isr_handler(void* arg)
{
    // Lecture des GPIO (méthode simple et fiable)
    uint8_t a = gpio_get_level(g_motor_config.pin_enc_a);
    uint8_t b = gpio_get_level(g_motor_config.pin_enc_b);
    uint8_t new_state = (a << 1) | b;
    
    // Décodage quadrature via lookup table
    uint8_t index = (g_last_state << 2) | new_state;
    int8_t delta = g_quadrature_lookup[index];
    
    // Mise à jour du compteur si transition valide
    if (delta != 0) {
        portENTER_CRITICAL_ISR(&g_encoder_spinlock);
        g_encoder_count += delta;
        g_last_pulse_time = esp_timer_get_time();
        portEXIT_CRITICAL_ISR(&g_encoder_spinlock);
    }
    
    g_last_state = new_state;
}

/**
 * @brief Configure les GPIO encodeur et enregistre l'ISR
 */
static int motor_setup_encoder_isr(void)
{
    ESP_LOGI(TAG, "Setup encodeur ISR: A=%d, B=%d",
             g_motor_config.pin_enc_a, g_motor_config.pin_enc_b);
    
    // Configuration GPIO pour les deux canaux encodeur
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << g_motor_config.pin_enc_a) | 
                        (1ULL << g_motor_config.pin_enc_b),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,      // Pull-up interne
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE         // Tous les fronts
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Installation du service ISR
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM | 
                                             ESP_INTR_FLAG_LEVEL3));
    
    // Enregistrement de l'ISR pour les deux GPIO
    ESP_ERROR_CHECK(gpio_isr_handler_add(g_motor_config.pin_enc_a, 
                                         encoder_isr_handler, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(g_motor_config.pin_enc_b, 
                                         encoder_isr_handler, NULL));
    
    // Lecture de l'état initial
    g_last_state = (gpio_get_level(g_motor_config.pin_enc_a) << 1) |
                   gpio_get_level(g_motor_config.pin_enc_b);
    g_last_pulse_time = esp_timer_get_time();
    
    ESP_LOGI(TAG, "Encodeur ISR initialisé (état=%d)", g_last_state);
    return 0;
}

/**
 * @brief Configure les GPIOs (broches)
 */
static int motor_setup_gpio(void)
{
    esp_err_t ret;
    
    // Configuration GPIO pour SLEEP (D5)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << g_motor_config.pin_sleep),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config GPIO SLEEP");
        return -1;
    }
    
    // Configuration GPIO pour DIR1 (D6) et DIR2 (D7)
    io_conf.pin_bit_mask = (1ULL << g_motor_config.pin_dir1) | (1ULL << g_motor_config.pin_dir2);
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config GPIO DIR");
        return -1;
    }
    
    // Configuration GPIO pour encodeur
    // motor_setup_encoder_isr() va les configurer avec les interruptions
    // Pas besoin de gpio_config ici, motor_setup_encoder_isr() le fait
    
    // Initialise l'encodeur avec ISR
    if (motor_setup_encoder_isr() != 0) {
        ESP_LOGE(TAG, "Erreur setup encodeur ISR");
        return -1;
    }
    
    return 0;
}

/**
 * @brief Configure le PWM pour IN1 et IN2
 * IN1 (DIR1) = LEDC_CHANNEL_0 pour direction UP
 * IN2 (DIR2) = LEDC_CHANNEL_1 pour direction DOWN
 */
static int motor_setup_pwm(void)
{
    esp_err_t ret;
    
    // Configuration du timer LEDC (partagé)
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config timer LEDC");
        return -1;
    }
    
    // Canal 0: IN1 (DIR1) pour direction UP
    ledc_channel_config_t ledc_channel_0 = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = g_motor_config.pin_dir1,
        .duty = 0,
        .hpoint = 0
    };
    ret = ledc_channel_config(&ledc_channel_0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config canal LEDC 0 (IN1)");
        return -1;
    }
    
    // Canal 1: IN2 (DIR2) pour direction DOWN
    ledc_channel_config_t ledc_channel_1 = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = g_motor_config.pin_dir2,
        .duty = 0,
        .hpoint = 0
    };
    ret = ledc_channel_config(&ledc_channel_1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config canal LEDC 1 (IN2)");
        return -1;
    }
    
    return 0;
}

/**
 * @brief Contrôle le moteur avec PWM sur IN1 (UP) ou IN2 (DOWN)
 * DEPLOY (UP): IN2=0, IN1=PWM (0→255)
 * RETRACT (DOWN): IN1=0, IN2=PWM (0→255)
 * 
 * @param duty Valeur PWM (0-255)
 * @param direction Direction du moteur (MOTOR_DIR_UP ou MOTOR_DIR_DOWN)
 */
static void motor_set_pwm(uint8_t duty, motor_direction_t direction)
{
    // duty est déjà uint8_t donc limité à 0-255
    
    if (direction == MOTOR_DIR_UP) {
        // DEPLOY: IN2=0, IN1=PWM
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);  // IN2 = 0
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);  // IN1 = PWM
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        // RETRACT: IN1=0, IN2=PWM
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);  // IN1 = 0
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);  // IN2 = PWM
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    }
}

/**
 * @brief Définit la direction et active le moteur
 */
static void motor_set_direction_and_enable(motor_direction_t direction)
{
    // Réveille le moteur
    gpio_set_level(g_motor_config.pin_sleep, 1);
    
    // Réinitialise les variables
    g_motor_phase = PHASE_ACCEL;
    g_pwm_duty = 0;  // Démarre à 0
    g_is_moving = true;
    g_move_direction = direction;
    
    ESP_LOGI(TAG, "Moteur démarrage: direction=%s",
             direction == MOTOR_DIR_UP ? "UP (DEPLOY)" : "DOWN (RETRACT)");
}

/**
 * @brief Tâche de rampe PWM simplifiée (2 phases)
 * 
 * Phase 1 (ACCEL): 0 → 255 (accélération rapide sur ~0.5s)
 * Phase 2 (CRUISE): 255 constant jusqu'à la fin (pas de décélération)
 */
static void motor_pwm_ramp_task(void *arg)
{
    while (1) {
        if (!g_is_moving) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        // PHASE 1: ACCÉLÉRATION (0 → 255)
        if (g_motor_phase == PHASE_ACCEL) {
            if (g_pwm_duty < PWM_MAX_DUTY) {
                g_pwm_duty += PWM_ACCEL_STEP;  // +5 tous les 10ms
                if (g_pwm_duty > PWM_MAX_DUTY) {
                    g_pwm_duty = PWM_MAX_DUTY;
                }
            } else {
                // Accélération terminée → passe en croisière
                g_motor_phase = PHASE_CRUISE;
                ESP_LOGI(TAG, "ACCEL→CRUISE: PWM=255");
            }
        }
        
        // PHASE 2: CROISIÈRE (255 constant jusqu'à la fin - DECEL désactivée)
        else if (g_motor_phase == PHASE_CRUISE) {
            g_pwm_duty = PWM_MAX_DUTY;  // Reste à 255 jusqu'au bout
        }
        
        // Applique le PWM au moteur
        motor_set_pwm(g_pwm_duty, g_move_direction);
        
        vTaskDelay(pdMS_TO_TICKS(10));  // Étape de 10ms
    }
}

int motor_manager_init(const motor_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Config NULL");
        return -1;
    }
    
    // Copie la configuration
    g_motor_config = *config;
    g_encoder_count = 0;
    g_motor_config.current_position = 0;
    g_motor_config.is_deployed = false;
    
    // Configure les GPIOs
    if (motor_setup_gpio() != 0) {
        ESP_LOGE(TAG, "Erreur setup GPIO");
        return -1;
    }
    
    // Configure le PWM
    if (motor_setup_pwm() != 0) {
        ESP_LOGE(TAG, "Erreur setup PWM");
        return -1;
    }
    
    // Met le moteur en mode sleep
    gpio_set_level(g_motor_config.pin_sleep, 0);
    gpio_set_level(g_motor_config.pin_dir1, 0);
    gpio_set_level(g_motor_config.pin_dir2, 0);
    
    // Lance la tâche de rampe PWM
    xTaskCreate(motor_pwm_ramp_task, "motor_pwm_ramp", 2048, NULL, 9, NULL);
    
    ESP_LOGI(TAG, "Gestionnaire moteur initialisé");
    ESP_LOGI(TAG, "Tours pour course complète: %.2f, Ratio: %.2f",
             config->turns_per_complete_travel, config->gear_ratio);
    
    return 0;
}

int motor_manager_turn(float n_turns_tige, motor_direction_t direction)
{
    if (g_is_moving) {
        ESP_LOGW(TAG, "Moteur déjà en mouvement");
        return -1;
    }
    
    // Calcule le nombre d'impulsions encodeur cibles
    // 48.57 tours × 150 (gear) × 7 PPR × 4 (quad) = 204000 comptes pour 34mm
    // Gear ratio corrigé: 1:150 selon datasheet moteur
    int32_t impulses = (int32_t)(n_turns_tige * g_motor_config.gear_ratio * 7);
    int32_t target_pulses = impulses * 4;  // ×4 quadrature
    
    // Reset des compteurs encodeur
    portENTER_CRITICAL(&g_encoder_spinlock);
    g_encoder_count = 0;
    g_last_state = (gpio_get_level(g_motor_config.pin_enc_a) << 1) |
                   gpio_get_level(g_motor_config.pin_enc_b);
    portEXIT_CRITICAL(&g_encoder_spinlock);
    
    g_move_target = target_pulses;
    g_move_direction = direction;
    
    // Lance le mouvement
    motor_set_direction_and_enable(direction);
    
    ESP_LOGI(TAG, "Démarrage rotation: %.2f tours, direction: %d, target: %ld impulsions",
             n_turns_tige, direction, target_pulses);
    
    g_is_moving = true;
    return 0;
}

/**
 * @brief Vérifie si la cible a été atteinte et arrête le moteur
 * Doit être appelée régulièrement depuis une tâche de monitoring
 * 
 * NOTE: La décélération progressive est gérée par motor_pwm_ramp_task()
 * Cette fonction vérifie juste si la cible est complètement atteinte
 */
void motor_manager_check_target(void)
{
    if (!g_is_moving || g_move_target == 0) {
        return;
    }
    
    int32_t current_pos = motor_manager_get_position();
    
    // Distance parcourue (valeur absolue)
    int32_t distance_traveled = (current_pos >= 0) ? current_pos : -current_pos;
    
    // Cible atteinte si distance parcourue >= target
    // Pas grave s'il y a un léger dépassement
    if (distance_traveled >= g_move_target) {
        ESP_LOGI(TAG, "Cible atteinte: %ld impulsions (target: %ld), arrêt",
                 distance_traveled, g_move_target);
        motor_manager_stop();
        g_is_moving = false;
        g_move_target = 0;
        g_motor_phase = PHASE_ACCEL;
    }
}

int motor_manager_deploy_video_proj(void)
{
    if (g_motor_config.is_deployed) {
        ESP_LOGW(TAG, "Vidéoprojecteur déjà déployé");
        return 0;
    }
    
    if (motor_manager_turn(g_motor_config.turns_per_complete_travel, MOTOR_DIR_UP) != 0) {
        return -1;
    }
    
    g_motor_config.is_deployed = true;
    g_motor_config.current_position = g_motor_config.turns_per_complete_travel;
    
    ESP_LOGI(TAG, "Déploiement du vidéoprojecteur lancé");
    return 0;
}

int motor_manager_retract_video_proj(void)
{
    if (!g_motor_config.is_deployed) {
        ESP_LOGW(TAG, "Vidéoprojecteur déjà rétracté");
        return 0;
    }
    
    if (motor_manager_turn(g_motor_config.turns_per_complete_travel, MOTOR_DIR_DOWN) != 0) {
        return -1;
    }
    
    g_motor_config.is_deployed = false;
    g_motor_config.current_position = 0;
    
    ESP_LOGI(TAG, "Rétraction du vidéoprojecteur lancée");
    return 0;
}

void motor_manager_stop(void)
{
    // Arrête les deux canaux PWM
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    
    // Met le moteur en sleep
    gpio_set_level(g_motor_config.pin_sleep, 0);
    
    g_is_moving = false;
    g_motor_config.current_position = g_encoder_count;
    
    ESP_LOGI(TAG, "Moteur arrêté, position: %ld impulsions", g_motor_config.current_position);
}

/**
 * @brief Contrôle le moteur avec PWM direct (sans rampe d'accélération)
 * Utile pour des tests ou des ajustements fins de vitesse
 * @param pwm Valeur PWM signée (-255 à +255)
 *            Positif = direction UP, Négatif = direction DOWN, 0 = arrêt
 */
void motor_manager_set_pwm(int16_t pwm)
{
    if (pwm == 0) {
        motor_manager_stop();
        return;
    }
    
    // Active le moteur
    gpio_set_level(g_motor_config.pin_sleep, 1);
    
    // Applique le PWM
    if (pwm > 0) {
        motor_set_pwm((uint8_t)pwm, MOTOR_DIR_UP);
    } else {
        motor_set_pwm((uint8_t)(-pwm), MOTOR_DIR_DOWN);
    }
}

bool motor_manager_is_deployed(void)
{
    return g_motor_config.is_deployed;
}

int32_t motor_manager_get_position(void)
{
    return g_encoder_count;
}

/**
 * @brief Fait un mouvement de réglage fin (jog)
 * Utilisé pour ajuster précisément la position du vidéoprojecteur
 */
int motor_manager_jog(float n_turns_tige, motor_direction_t direction)
{
    if (g_is_moving) {
        ESP_LOGW(TAG, "Moteur déjà en mouvement");
        return -1;
    }
    
    ESP_LOGI(TAG, "JOG: %.2f tours tige, direction=%s",
             n_turns_tige, direction == MOTOR_DIR_UP ? "UP" : "DOWN");
    
    // Utilise motor_manager_turn pour faire le mouvement
    return motor_manager_turn(n_turns_tige, direction);
}
