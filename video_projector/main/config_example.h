/**
 * @file config_example.h
 * @brief Exemples de configuration système
 * 
 * Copiez ce fichier en config.h et ajustez les paramètres selon votre setup
 */

#ifndef CONFIG_EXAMPLE_H
#define CONFIG_EXAMPLE_H

// =====================================================================
// PARAMÈTRES MOTEUR & MÉCANIQUE
// =====================================================================

/**
 * Tours de l'encodeur rotatif pour une course complète
 * 
 * Exemple: Si votre tige filetée a:
 * - Pas = 2mm
 * - Engrenage moteur = 100:1
 * - Encodeur = 12 PPR (impulsions par révolution)
 * 
 * Calcul:
 * - 1 tour moteur = 2mm de déplacement
 * - 1 tour tige = 100 tours moteur = 200mm
 * - Encodeur: 12 impulsions par tour moteur
 * - Pour 200mm: 200 tours moteur * 12 = 2400 impulsions
 * 
 * Ajustez selon votre système mécanique!
 */
#define MOTOR_CONFIG_TURNS_COMPLETE_TRAVEL 10.0f

/**
 * Ratio de réduction moteur/tige
 * 
 * Rapport entre la vitesse moteur et la vitesse de sortie
 * Exemple:
 * - Engrenage 100:1 = ratio 100
 * - Poulie 30 dents → 120 dents = ratio 4
 */
#define MOTOR_CONFIG_GEAR_RATIO 100.0f

// =====================================================================
// PARAMÈTRES BOUTON
// =====================================================================

/**
 * Temps de debounce (millisecondes)
 * Attend ce temps avant de confirmer un appui du bouton
 */
#define BUTTON_CONFIG_DEBOUNCE_MS 50

/**
 * Durée minimale pour un appui long (millisecondes)
 * Appuis >= cette durée = appui long
 */
#define BUTTON_CONFIG_LONG_PRESS_MS 1000

// =====================================================================
// PARAMÈTRES LED IR
// =====================================================================

/**
 * Fréquence porteuse IR en Hz
 * 
 * Valeurs standard par constructeur:
 * - 38 kHz: NEC, Sony, Philips (MAJORITÉ)
 * - 36 kHz: Panasonic
 * - 40 kHz: Samsung
 * - 56 kHz: Certain appareils
 * 
 * 38000 Hz est le plus courant
 */
#define IR_CONFIG_FREQUENCY_HZ 38000

/**
 * Rapport cyclique du signal PWM (%)
 * Généralement 50% pour les LED IR
 */
#define IR_CONFIG_DUTY_CYCLE 50

// =====================================================================
// PARAMÈTRES BLE
// =====================================================================

/**
 * Nom du device BLE (max 31 caractères pour HCI)
 */
#define BLE_CONFIG_DEVICE_NAME "VideoProjector_Van"

/**
 * Intervalle d'advertising BLE (millisecondes)
 * Plus court = découverte plus rapide mais plus de consommation
 */
#define BLE_CONFIG_ADVERTISING_INTERVAL_MS 500

/**
 * Nombre maximum de connexions BLE simultanées
 * (actuellement limité à 1 pour simplifier)
 */
#define BLE_CONFIG_MAX_CONNECTIONS 1

// =====================================================================
// PARAMÈTRES AVANCÉS
// =====================================================================

/**
 * Mode de logging par défaut
 * 
 * 0 = Aucun log
 * 1 = Erreurs (ERROR)
 * 2 = Avertissements (WARN)
 * 3 = Infos (INFO) - RECOMMANDÉ
 * 4 = Debug (DEBUG)
 * 5 = Verbose (VERBOSE)
 */
#define LOG_CONFIG_LEVEL 3

/**
 * Intervalle de monitoring moteur (ms)
 * Fréquence de vérification de la position
 */
#define MOTOR_CONFIG_MONITOR_INTERVAL_MS 100

/**
 * Intervalle de monitoring USB (ms)
 * Fréquence de vérification de l'alimentation USB
 */
#define USB_CONFIG_MONITOR_INTERVAL_MS 100

// =====================================================================
// PARAMÈTRES DE PERFORMANCE
// =====================================================================

/**
 * Priorité des tâches FreeRTOS
 * 
 * 0 = Plus basse
 * 24 = ConfigMAX_PRIORITIES - 1 (plus haute)
 * 
 * Recommandé:
 * - Motor monitor: 5
 * - Button task: 10
 * - USB monitor: 5
 * - BLE task: 15-20
 */
#define FREERTOS_TASK_PRIORITY_MOTOR    5
#define FREERTOS_TASK_PRIORITY_BUTTON   10
#define FREERTOS_TASK_PRIORITY_USB      5
#define FREERTOS_TASK_PRIORITY_BLE      15

/**
 * Tailles des stacks des tâches (en octets)
 * 
 * Minimum recommandé:
 * - Tâche simple: 2048
 * - Tâche avec allocations: 4096
 * - Tâche BLE: 4096+
 */
#define FREERTOS_STACK_SIZE_MOTOR       2048
#define FREERTOS_STACK_SIZE_BUTTON      2048
#define FREERTOS_STACK_SIZE_USB         2048
#define FREERTOS_STACK_SIZE_BLE_HOST    4096

// =====================================================================
// EXAMPLE D'ADAPTATION POUR VOTRE PROJET
// =====================================================================

/**
 * EXEMPLE 1: Tige filetée pas 2mm, moteur 12V 30RPM, réducteur 100:1
 * 
 * 1 rotation moteur = 2mm déplacement
 * 30 RPM moteur = 0.3 RPM tige
 * Encodeur 12 PPR → 12*100 = 1200 impulsions par tour tige
 * 
 * Pour course 200mm (100 tours tige):
 * 100 tours * 1200 impulsions = 120,000 impulsions
 * 
 * #define MOTOR_CONFIG_TURNS_COMPLETE_TRAVEL 120000 / 12 = 10000
 */

/**
 * EXEMPLE 2: Tige filetée pas 5mm, moteur 24V 60RPM, poulie 30→120
 * 
 * Ratio poulie = 4 (pas de réducteur mécanique)
 * Encodeur 20 PPR → 20*4 = 80 impulsions par tour tige
 * 
 * Pour course 500mm (100 tours tige):
 * 100 tours * 80 impulsions = 8000 impulsions
 * 
 * #define MOTOR_CONFIG_TURNS_COMPLETE_TRAVEL 8000 / 20 = 400
 */

#endif // CONFIG_EXAMPLE_H
