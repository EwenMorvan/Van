#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "ble_comm.h"

static const char *TAG = "MAIN3";

// ⚙️ CONFIGURATION : Choisir le mode ici
// Changez cette valeur pour sélectionner le mode :
// true  = ESP A (SERVEUR)
// false = ESP B (CLIENT)
#define IS_ESP_A_SERVER false

// Variables globales
static ble_comm_t ble_comm;
static bool is_connected = false;

// Callback quand des données sont reçues
void on_data_received(const char* data, size_t len) {
    ESP_LOGI(TAG, "📨 Received: %.*s", (int)len, data);
}

// Callback quand l'état de connexion change
void on_connection_change(bool connected) {
    is_connected = connected;
    if (connected) {
        ESP_LOGI(TAG, "🔗 BLE Connected!");
    } else {
        ESP_LOGW(TAG, "🔌 BLE Disconnected!");
    }
}

// Tâche ESP A (Serveur)
static void task_esp_a(void *arg) {
    ESP_LOGI(TAG, "🚀 Starting ESP A (SERVER)");
    
    // Initialiser BLE comme serveur
    esp_err_t ret = ble_comm_init(&ble_comm, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to init BLE server: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    // Configurer les callbacks
    ble_comm_set_data_callback(on_data_received);
    ble_comm_set_connection_callback(on_connection_change);
    
    // Démarrer le serveur
    ret = ble_comm_start(&ble_comm);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to start BLE server: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "✅ BLE Server started, waiting for client...");
    
    int message_counter = 0;
    char message[128];
    
    while (1) {
        // Attendre la connexion
        if (is_connected) {
            // Envoyer un message périodique
            snprintf(message, sizeof(message), "Hello from ESP-A (Server) - Message #%d", ++message_counter);
            
            ret = ble_comm_send(&ble_comm, message, strlen(message));
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "📤 Sent: %s", message);
            } else {
                ESP_LOGE(TAG, "❌ Failed to send message: %s", esp_err_to_name(ret));
            }
            
            // Vérifier si on a reçu des données
            char rx_buffer[256];
            size_t rx_len;
            ret = ble_comm_recv(&ble_comm, rx_buffer, sizeof(rx_buffer), &rx_len);
            if (ret == ESP_OK && rx_len > 0) {
                ESP_LOGI(TAG, "📨 Received response: %s", rx_buffer);
            }
            
            vTaskDelay(pdMS_TO_TICKS(5000)); // Envoyer toutes les 5 secondes
        } else {
            ESP_LOGI(TAG, "⏳ Waiting for client connection...");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

// Tâche ESP B (Client)
static void task_esp_b(void *arg) {
    ESP_LOGI(TAG, "🚀 Starting ESP B (CLIENT)");
    
    // Attendre un peu pour éviter les conflits d'initialisation
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Initialiser BLE comme client
    esp_err_t ret = ble_comm_init(&ble_comm, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to init BLE client: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    // Configurer les callbacks
    ble_comm_set_data_callback(on_data_received);
    ble_comm_set_connection_callback(on_connection_change);
    
    // Démarrer le client (scan)
    ret = ble_comm_start(&ble_comm);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to start BLE client: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "✅ BLE Client started, scanning for server...");
    
    int response_counter = 0;
    char response[128];
    
    while (1) {
        if (is_connected) {
            // Envoyer un message périodique au serveur
            snprintf(response, sizeof(response), "Message from ESP-B Client #%d", ++response_counter);
            ret = ble_comm_send(&ble_comm, response, strlen(response));
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "📤 Sent to server: %s", response);
            } else {
                ESP_LOGE(TAG, "❌ Failed to send to server: %s", esp_err_to_name(ret));
            }
            
            // Vérifier si on a reçu des données du serveur
            char rx_buffer[256];
            size_t rx_len;
            ret = ble_comm_recv(&ble_comm, rx_buffer, sizeof(rx_buffer), &rx_len);
            if (ret == ESP_OK && rx_len > 0) {
                ESP_LOGI(TAG, "📨 Received from server: %s", rx_buffer);
            }
            
            vTaskDelay(pdMS_TO_TICKS(3000)); // Envoyer un message toutes les 3 secondes
        } else {
            ESP_LOGI(TAG, "🔍 Scanning for server...");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "🌟 Starting BLE Communication Demo");
    
    // Initialiser NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "📋 NVS Flash initialized");
    
    // Démarrer selon la configuration définie en haut du fichier
    if (IS_ESP_A_SERVER) {
        ESP_LOGI(TAG, "🅰️  Running as ESP A (SERVER)");
        xTaskCreate(task_esp_a, "esp_a_task", 4096, NULL, 5, NULL);
    } else {
        ESP_LOGI(TAG, "🅱️  Running as ESP B (CLIENT)");
        xTaskCreate(task_esp_b, "esp_b_task", 4096, NULL, 5, NULL);
    }

    ESP_LOGI(TAG, "✅ Main task completed, BLE communication running...");
}

/*
 * 📋 INSTRUCTIONS D'UTILISATION :
 * 
 * 1. Choisir le mode en modifiant IS_ESP_A_SERVER en haut du fichier :
 *    - true  = ESP A (SERVEUR) 
 *    - false = ESP B (CLIENT)
 * 
 * 2. Compiler et flasher :
 *    idf.py build flash monitor
 * 
 * 3. Pour changer de mode sur une autre carte :
 *    - Modifier IS_ESP_A_SERVER dans le code
 *    - Recompiler : idf.py build flash monitor
 * 
 * 4. Surveillance des logs :
 *    idf.py monitor
 * 
 * 🔧 FONCTIONNEMENT :
 * - ESP A (serveur) : attend les connexions et envoie des messages périodiques
 * - ESP B (client) : scanne, se connecte au serveur et répond aux messages  
 * - Reconnexion automatique en cas de déconnexion
 * - Logs détaillés pour le debugging avec émojis
 * 
 * 💡 EXEMPLE D'USAGE :
 * - Carte 1 : IS_ESP_A_SERVER = true  (serveur)
 * - Carte 2 : IS_ESP_A_SERVER = false (client)  
 * - Allumer les deux cartes, elles se connecteront automatiquement
 */
