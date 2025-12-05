#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "motor_manager.h"
#include "button_manager.h"
#include "usb_manager.h"
#include "ir_led_manager.h"
#include "ble_manager.h"

static const char *TAG = "MAIN";

// Configuration des broches pour Seeed Xiao ESP32-C3

#define PIN_MOTOR_SLEEP    GPIO_NUM_7  // D5 → GPIO7 (Moteur SLEEP)
#define PIN_MOTOR_DIR2     GPIO_NUM_21   // D6 → GPIO21  (Moteur DIR2)
#define PIN_MOTOR_DIR1     GPIO_NUM_20   // D7 → GPIO20  (Moteur DIR1)
#define PIN_ENC_A          GPIO_NUM_4  // D2 → GPIO4 (Encodeur A)
#define PIN_ENC_B          GPIO_NUM_5  // D3 → GPIO5 (Encodeur B)
#define PIN_BUTTON         GPIO_NUM_9   // D9 → GPIO9  (Bouton)
#define PIN_BUTTON_LED     GPIO_NUM_10   // D10→ GPIO10  (LED Bouton)
#define PIN_USB_FLAG       GPIO_NUM_3   // D1 → GPIO3  (Drapeau USB)
#define PIN_IR_LED         GPIO_NUM_8   // D8 → GPIO8  (LED IR)

// Paramètres système (à ajuster selon votre mécanique)
// Moteur N20: 7 PPR encodeur Hall sur arbre moteur
// Cinématique: Arbre moteur (rapide) → [Réducteur 1/100] → Tige filetée (lente, 0.7mm/tour)

#define TURNS_PER_COMPLETE_TRAVEL 55.0f  // Tours arbre moteur pour 34mm

#define GEAR_RATIO                150.0f   // Rapport réducteur 1:150 (multiplicateur)

#define PERIODICALY_SEND_BLE_STATUS 0  // Envoi état BLE périodiquement (1=oui, 0=non) si 0 uniquement sur changement état ou requête
#define BLE_STATUS_INTERVAL_MS 500  // Intervalle en ms pour envoi périodique

// Variables globales d'état
static bool g_is_usb_powered = false;
static motor_config_t g_motor_config;

// Variable pour clignotement LED pendant mouvement
static bool g_led_blink = false;

// Prototypes des callbacks
static void button_event_callback(button_event_t event);
static void usb_power_callback(bool is_powered);
static void ble_command_callback(ble_command_t cmd);
static void motor_state_callback(motor_state_t new_state, float position_percent);

// Variable globale pour éviter envoi BLE depuis callback (risque stack overflow)
static motor_state_t g_last_state = MOTOR_STATE_RETRACTED;
static bool g_state_changed = false;

/**
 * @brief Callback d'état moteur - Met à jour la LED (léger, safe pour callback)
 */
static void motor_state_callback(motor_state_t new_state, float position_percent)
{
    // Synchronisation LED bouton selon l'état
    switch (new_state) {
        case MOTOR_STATE_RETRACTED:
            g_led_blink = false;
            button_manager_set_led(false);
            break;
        case MOTOR_STATE_DEPLOYING:
        case MOTOR_STATE_RETRACTING:
            g_led_blink = true;  // Clignotement pendant mouvement
            break;
        case MOTOR_STATE_DEPLOYED:
            g_led_blink = false;
            button_manager_set_led(true);
            break;
        case MOTOR_STATE_STOPPED:
            g_led_blink = false;
            button_manager_set_led(false);  // Ou selon position, mais pour l'instant off
            break;
    }
    
    // Marque le changement d'état pour envoi BLE depuis la tâche monitor
    g_last_state = new_state;
    g_state_changed = true;
}

/**
 * @brief Callback du bouton - Gère le déploiement/rétraction du vidéoprojecteur
 */
static void button_event_callback(button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_SHORT_PRESS:
            ESP_LOGI(TAG, "Bouton: Appui court détecté");
            
            // Si STOPPED, débloquer vers la position la plus proche
            if (motor_manager_get_state() == MOTOR_STATE_STOPPED) {
                float current_percent = motor_manager_get_position_percent();
                if (current_percent >= 50.0f) {
                    ESP_LOGI(TAG, "Déblocage depuis STOPPED: deploy vers 100%%");
                    motor_manager_deploy_video_proj();
                } else {
                    ESP_LOGI(TAG, "Déblocage depuis STOPPED: retract vers 0%%");
                    motor_manager_retract_video_proj();
                }
                return;
            }
            
            // Toggle du vidéoprojecteur
            if (motor_manager_is_deployed()) {
                ESP_LOGI(TAG, "Rétraction du vidéoprojecteur");
                motor_manager_retract_video_proj();
            } else {
                // Vérifier que l'alimentation USB est coupée avant déploiement
                // TEMPORAIRE: désactivé pour tests
                // if (g_is_usb_powered) {
                //     ESP_LOGW(TAG, "Impossible de déployer: vidéoprojecteur encore alimenté");
                //     button_manager_set_led(false);
                //     return;
                // }
                
                ESP_LOGI(TAG, "Déploiement du vidéoprojecteur");
                motor_manager_deploy_video_proj();
            }
            break;
            
        case BUTTON_EVENT_LONG_PRESS:
            ESP_LOGI(TAG, "Bouton: Appui long détecté - arrêt d'urgence");
            motor_manager_stop();
            motor_manager_set_state(MOTOR_STATE_STOPPED);
            break;
            
        case BUTTON_EVENT_RELEASED:
            // Action optionnelle à la relâche
            break;
    }
}

/**
 * @brief Callback d'alimentation USB - Empêche la rétraction si le vidéoproj est alimenté
 */
static void usb_power_callback(bool is_powered)
{
    g_is_usb_powered = is_powered;
    
    if (is_powered) {
        ESP_LOGW(TAG, "⚠️  Vidéoprojecteur alimenté via USB - rétraction bloquée");
        motor_manager_stop();
        motor_manager_set_state(MOTOR_STATE_STOPPED);
    } else {
        ESP_LOGI(TAG, "✓ USB désalimenté - rétraction possible");
    }
}

/**
 * @brief Callback BLE - Gère les commandes reçues via BLE
 */
static void ble_command_callback(ble_command_t cmd)
{
    switch (cmd) {
        case BLE_CMD_DEPLOY:
            ESP_LOGI(TAG, "BLE: DEPLOY");
            if (g_is_usb_powered) {
                ESP_LOGW(TAG, "Impossible: vidéoprojecteur alimenté");
            } else {
                motor_manager_deploy_video_proj();
            }
            break;
            
        case BLE_CMD_RETRACT:
            ESP_LOGI(TAG, "BLE: RETRACT");
            motor_manager_retract_video_proj();
            break;
            
        case BLE_CMD_STOP:
            ESP_LOGI(TAG, "BLE: STOP");
            motor_manager_stop();
            motor_manager_set_state(MOTOR_STATE_STOPPED);
            break;
            
        case BLE_CMD_GET_STATUS:
            ESP_LOGI(TAG, "BLE: GET_STATUS");
            g_state_changed = true;  // Force l'envoi de l'état actuel
            break;
        
        // Commandes de réglage fin (JOG)
        case BLE_CMD_JOG_UP_1:
            ESP_LOGI(TAG, "BLE: JOG UP +1.0 tour");
            motor_manager_jog(1.0f, MOTOR_DIR_UP);
            break;
            
        case BLE_CMD_JOG_UP_01:
            ESP_LOGI(TAG, "BLE: JOG UP +0.1 tour");
            motor_manager_jog(0.1f, MOTOR_DIR_UP);
            break;
            
        case BLE_CMD_JOG_UP_001:
            ESP_LOGI(TAG, "BLE: JOG UP +0.01 tour");
            motor_manager_jog(0.01f, MOTOR_DIR_UP);
            break;
            
        case BLE_CMD_JOG_DOWN_1:
            ESP_LOGI(TAG, "BLE: JOG DOWN -1.0 tour");
            motor_manager_jog(1.0f, MOTOR_DIR_DOWN);
            break;
            
        case BLE_CMD_JOG_DOWN_01:
            ESP_LOGI(TAG, "BLE: JOG DOWN -0.1 tour");
            motor_manager_jog(0.1f, MOTOR_DIR_DOWN);
            break;
            
        case BLE_CMD_JOG_DOWN_001:
            ESP_LOGI(TAG, "BLE: JOG DOWN -0.01 tour");
            motor_manager_jog(0.01f, MOTOR_DIR_DOWN);
            break;
        
        // Commandes sans limites
        case BLE_CMD_JOG_UP_UNLIMITED:
            ESP_LOGI(TAG, "BLE: JOG UP UNLIMITED +1.0 tour");
            motor_manager_jog_unlimited(1.0f, MOTOR_DIR_UP);
            break;
            
        case BLE_CMD_JOG_DOWN_UNLIMITED:
            ESP_LOGI(TAG, "BLE: JOG DOWN UNLIMITED -1.0 tour");
            motor_manager_jog_unlimited(1.0f, MOTOR_DIR_DOWN);
            break;
        
        // Commandes de calibration
        case BLE_CMD_CALIB_UP:
            ESP_LOGI(TAG, "BLE: CALIBRATE UP (force 100%)");
            motor_manager_calibrate_up();
            break;
            
        case BLE_CMD_CALIB_DOWN:
            ESP_LOGI(TAG, "BLE: CALIBRATE DOWN (force 0%)");
            motor_manager_calibrate_down();
            break;
            
        default:
            ESP_LOGW(TAG, "BLE: Commande inconnue %d", cmd);
            break;
    }
}

/**
 * @brief Tâche de clignotement de la LED pendant les mouvements
 */
static void led_blink_task(void *pvParameters)
{
    bool led_state = false;
    while (1) {
        if (g_led_blink) {
            led_state = !led_state;
            button_manager_set_led(led_state);
        }
        vTaskDelay(pdMS_TO_TICKS(500));  // Clignotement toutes les 500ms
    }
}

/**
 * @brief Tâche de monitoring du moteur
 * Vérifie régulièrement la position et arrête le moteur si nécessaire
 * Envoie aussi les mises à jour d'état via BLE (régulièrement et sur changement)
 */
static void motor_monitor_task(void *pvParameters)
{
    static char json_buf[128];
    static uint32_t ble_update_counter = 0;
    static const TickType_t delay = 10;  // Délai de 10ms entre chaque itération
    
    while (1) {
        // Vérifie si la cible a été atteinte et arrête le moteur
        motor_manager_check_target();
        
        // Envoi BLE : sur changement d'état OU périodiquement toutes les 2 secondes
        bool should_send_ble = false;
        
        if (g_state_changed) {
            g_state_changed = false;
            should_send_ble = true;
        } else if (PERIODICALY_SEND_BLE_STATUS && ble_manager_is_connected() && (ble_update_counter++ % (BLE_STATUS_INTERVAL_MS / delay) == 0)) {
            // Envoi périodique toutes les BLE_STATUS_INTERVAL_MS millisecondes
            should_send_ble = true;
        }
        
        if (should_send_ble && ble_manager_is_connected()) {
            const char *state_str;
            motor_state_t current_state = motor_manager_get_state();
            
            switch (current_state) {
                case MOTOR_STATE_RETRACTED:   state_str = "RETRACTED"; break;
                case MOTOR_STATE_DEPLOYING:   state_str = "DEPLOYING"; break;
                case MOTOR_STATE_DEPLOYED:    state_str = "DEPLOYED"; break;
                case MOTOR_STATE_RETRACTING:  state_str = "RETRACTING"; break;
                case MOTOR_STATE_STOPPED:     state_str = "STOPPED"; break;
                default:                      state_str = "UNKNOWN"; break;
            }
            
            float position_percent = motor_manager_get_position_percent();
            snprintf(json_buf, sizeof(json_buf),
                     "{\"state\":\"%s\",\"position\":%.2f}",
                     state_str, position_percent);
            
            ESP_LOGI(TAG, "📡 État BLE: %s (%.2f%%)", state_str, position_percent);
            ble_manager_send_json(json_buf);
        }
        
        // Log périodique de la position (toutes les 5 secondes)
        static uint32_t log_counter = 0;
        if (log_counter++ % 50 == 0) {
            int32_t current_pos = motor_manager_get_position();
            ESP_LOGI(TAG, "Position moteur: %ld impulsions", current_pos);
        }
        
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}

/**
 * @brief Initialisation complète du système
 */
static void system_init(void)
{
    int rc;
    
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "Système Vidéoprojecteur Motorisé");
    ESP_LOGI(TAG, "ESP32-C3 Xiao");
    ESP_LOGI(TAG, "================================");
    
    // Configuration du moteur
    g_motor_config.pin_sleep = PIN_MOTOR_SLEEP;
    g_motor_config.pin_dir1 = PIN_MOTOR_DIR1;
    g_motor_config.pin_dir2 = PIN_MOTOR_DIR2;
    g_motor_config.pin_enc_a = PIN_ENC_A;
    g_motor_config.pin_enc_b = PIN_ENC_B;
    g_motor_config.turns_per_complete_travel = TURNS_PER_COMPLETE_TRAVEL;
    g_motor_config.gear_ratio = GEAR_RATIO;
    
    // Initialise le gestionnaire moteur
    ESP_LOGI(TAG, "Initialisation du gestionnaire moteur...");
    rc = motor_manager_init(&g_motor_config);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur initialisation moteur");
        return;
    }
    
    // Enregistre le callback d'état moteur
    motor_manager_set_state_callback(motor_state_callback);
    
    // Initialise le gestionnaire bouton
    ESP_LOGI(TAG, "Initialisation du gestionnaire bouton...");
    rc = button_manager_init(PIN_BUTTON, PIN_BUTTON_LED, button_event_callback);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur initialisation bouton");
        return;
    }
    
    // Synchronise la LED du bouton avec l'état restauré depuis NVS
    motor_state_t initial_state = motor_manager_get_state();
    bool led_state = (initial_state == MOTOR_STATE_DEPLOYED);
    button_manager_set_led(led_state);
    ESP_LOGI(TAG, "LED bouton synchronisée avec état restauré: %s", 
             led_state ? "ON (DEPLOYED)" : "OFF (RETRACTED)");
    
    // Initialise le gestionnaire USB
    ESP_LOGI(TAG, "Initialisation du gestionnaire USB...");
    rc = usb_manager_init(PIN_USB_FLAG, usb_power_callback);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur initialisation USB");
        return;
    }
    
    // Initialise le gestionnaire LED IR
    ESP_LOGI(TAG, "Initialisation du gestionnaire LED IR...");
    ir_config_t ir_config = {
        .frequency = 38000,  // Fréquence standard IR (38 kHz)
        .duty_cycle = 50
    };
    rc = ir_led_manager_init(PIN_IR_LED, &ir_config);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur initialisation LED IR");
        return;
    }
    
    // Initialise le gestionnaire BLE
    ESP_LOGI(TAG, "Initialisation du gestionnaire BLE...");
    rc = ble_manager_init("VideoProjector_Van", ble_command_callback);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur initialisation BLE");
        return;
    }
    
    // Crée la tâche de monitoring du moteur (4096 bytes pour les callbacks BLE)
    xTaskCreate(motor_monitor_task, "motor_monitor", 4096, NULL, 5, NULL);
    
    // Crée la tâche de clignotement LED
    xTaskCreate(led_blink_task, "led_blink", 2048, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "✓ Système initialisé avec succès!");
    ESP_LOGI(TAG, "================================");
}

void app_main(void)
{
    nvs_flash_init();
    // Initialise le système
    system_init();

    
    // La boucle principale est gérée par les tâches FreeRTOS
    // Cette tâche reste en attente passive
    while (1) {
        // To dedegub print motor_state_t
        motor_state_t current_state = motor_manager_get_state();
        ESP_LOGI(TAG, "État moteur actuel: %d", current_state);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
