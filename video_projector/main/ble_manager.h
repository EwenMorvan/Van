#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BLE_CMD_DEPLOY,           // 0
    BLE_CMD_RETRACT,          // 1
    BLE_CMD_STOP,             // 2
    BLE_CMD_GET_STATUS,       // 3
    // Commandes de réglage fin (alternance UP/DOWN)
    BLE_CMD_JOG_UP_1,         // 4: +1.0 tour
    BLE_CMD_JOG_DOWN_1,       // 5: -1.0 tour
    BLE_CMD_JOG_UP_01,        // 6: +0.1 tour
    BLE_CMD_JOG_DOWN_01,      // 7: -0.1 tour
    BLE_CMD_JOG_UP_001,       // 8: +0.01 tour
    BLE_CMD_JOG_DOWN_001,     // 9: -0.01 tour
    // Commandes sans limites (pour dépasser 0-100%)
    BLE_CMD_JOG_UP_UNLIMITED, // 10: +1.0 tour sans limite
    BLE_CMD_JOG_DOWN_UNLIMITED, // 11: -1.0 tour sans limite
    // Commandes de calibration (force position sans bouger)
    BLE_CMD_CALIB_UP,         // 12: Force position à 100%
    BLE_CMD_CALIB_DOWN        // 13: Force position à 0%
} ble_command_t;

typedef void (*ble_command_callback_t)(ble_command_t cmd);

/**
 * @brief Initialise le gestionnaire BLE NimBLE
 * @param device_name Nom du device BLE
 * @param callback Fonction appelée lors de la réception d'une commande
 * @return 0 si succès, -1 si erreur
 */
int ble_manager_init(const char *device_name, ble_command_callback_t callback);

/**
 * @brief Démarre l'advertising BLE
 * @return 0 si succès, -1 si erreur
 */
int ble_manager_start_advertising(void);

/**
 * @brief Arrête l'advertising BLE
 * @return 0 si succès, -1 si erreur
 */
int ble_manager_stop_advertising(void);

/**
 * @brief Envoie l'état du vidéoprojecteur aux clients connectés
 * @param is_deployed true si déployé, false sinon
 * @return 0 si succès, -1 si erreur
 */
int ble_manager_notify_status(bool is_deployed);

/**
 * @brief Envoie des données JSON aux clients connectés
 * @param json_string Chaîne JSON à envoyer
 * @return 0 si succès, -1 si erreur
 */
int ble_manager_send_json(const char *json_string);

/**
 * @brief Vérifie si un client BLE est connecté
 * @return true si connecté, false sinon
 */
bool ble_manager_is_connected(void);

#endif // BLE_MANAGER_H
