#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ble_comm.h"

static const char *TAG = "BLE_TEST";

// Variables globales
static ble_comm_t ble_comm;
static bool is_connected = false;

// Callback quand des données sont reçues
void on_data_received(const char* data, size_t len) {
    ESP_LOGI(TAG, "📨 Received from server: %.*s", (int)len, data);
}

// Callback quand l'état de connexion change
void on_connection_change(bool connected) {
    is_connected = connected;
    if (connected) {
        ESP_LOGI(TAG, "🔗 Connected to BLE server!");
    } else {
        ESP_LOGW(TAG, "🔌 Disconnected from BLE server!");
    }
}

// Tâche ESP32 Client pour tester la connexion au serveur dual-service
static void esp32_client_task(void *arg) {
    ESP_LOGI(TAG, "🚀 Starting ESP32 Client (connecting to dual-service server)");
    
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
    
    ESP_LOGI(TAG, "✅ BLE Client started, scanning for dual-service server...");
    ESP_LOGI(TAG, "🔍 Looking for server with services:");
    ESP_LOGI(TAG, "   📱 Van Management (0xAAA0) - for smartphones");
    ESP_LOGI(TAG, "   🔌 ESP32 Communication (0x1234) - for ESP32 clients");
    
    int message_counter = 0;
    char message[128];
    
    while (1) {
        if (is_connected) {
            // Vérifier si on a reçu des données du serveur
            char rx_buffer[256];
            size_t rx_len;
            ret = ble_comm_recv(&ble_comm, rx_buffer, sizeof(rx_buffer), &rx_len);
            if (ret == ESP_OK && rx_len > 0) {
                ESP_LOGI(TAG, "📨 Processed server message: %s", rx_buffer);
                
                // Envoyer une réponse périodique au serveur  
                if (message_counter % 3 == 0) {  // Toutes les 3 réceptions
                    snprintf(message, sizeof(message), 
                             "ESP32-Client-Response #%d: Server message received OK", 
                             ++message_counter);
                    
                    // Note: Pour l'instant, client->server n'est pas implémenté
                    // Le serveur peut envoyer au client via notifications
                    ESP_LOGI(TAG, "📤 Would send to server: %s", message);
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            ESP_LOGI(TAG, "🔍 Scanning for dual-service server...");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "🌟 Starting Dual-Service BLE Client Test");
    ESP_LOGI(TAG, "🎯 This client will connect to a server that supports:");
    ESP_LOGI(TAG, "   📱 Smartphones (Van Management Service)");
    ESP_LOGI(TAG, "   🔌 ESP32 clients (ESP32 Communication Service)");
    
    // Initialiser NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "📋 NVS Flash initialized");
    
    // Démarrer la tâche client
    xTaskCreate(esp32_client_task, "esp32_client_task", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "✅ Client task started, connecting to dual-service server...");
}

/*
 * 📋 TEST PROCEDURE:
 * 
 * 1. Flash this code on ESP32 #1 (client)
 * 2. Flash ble_server.c code on ESP32 #2 (dual-service server)  
 * 3. Monitor both devices
 * 
 * 🔧 EXPECTED BEHAVIOR:
 * - ESP32 #2 (server): Advertises with both services (0xAAA0 + 0x1234)
 * - ESP32 #1 (client): Finds and connects to server
 * - Server can send data to client via ESP32 Communication Service
 * - Server can also accept smartphone connections via Van Management Service
 * - Multiple connections supported simultaneously
 * 
 * 📱 SMARTPHONE TEST:
 * - Use a BLE scanner app to find "VanManagement" device
 * - Connect and explore services 0xAAA0 (Van) and 0x1234 (ESP32)
 * - Both ESP32 client and smartphone can be connected simultaneously
 */
