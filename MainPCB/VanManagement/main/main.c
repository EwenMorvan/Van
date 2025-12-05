#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "../common_includes/simulation_config.h"

#include "global_coordinator.h"
#include "../communications/protocol.h"
#include "../communications/ble/ble_manager_nimble.h"
#include "../communications/ble/fragment_handler.h"
#include "../communications/uart/uart_multiplexer.h"
#include "../communications/slave_main_communication_manager.h"
#include "../communications/app_main_communication_manager.h"
#include "../communications/command_parser.h"
#include "../peripherals_devices/switch_manager.h"
#include "../peripherals_devices/led_manager.h"
#include "../peripherals_devices/led_coordinator.h"
#include "../peripherals_devices/led_command_handler.h"
#include "../peripherals_devices/hood_manager.h"
#include "../peripherals_devices/heater_manager.h"
#include "../peripherals_devices/battery_manager.h"
#include "../peripherals_devices/mppt_manager.h"
#include "../peripherals_devices/inverter_chargers_manager.h"
#include "../peripherals_devices/energy_simulation.h"
#include "../peripherals_devices/videoprojecteur_manager.h"
#include "../peripherals_devices/htco2_sensor_manager.h"
#include "../utils/battery_parser.h"

static const char *TAG = "MAIN";

#define PRINT_DEBUG_VAN_STATE 0

// ============================================================================
// COMMAND EXECUTION FUNCTIONS
// ============================================================================

// Mutex pour gérer les commandes simultanées provenant de plusieurs apps
static SemaphoreHandle_t g_command_mutex = NULL;

/**
 * Fonction de traitement des commandes (à implémenter selon votre logique)
 * Note: Si plusieurs commandes arrivent simultanément de différentes apps,
 * seule la première est traitée grâce au mutex.
 */
void handle_van_command(van_command_t* cmd) {
    // Tenter d'acquérir le mutex (si occupé, ignorer la commande)
    if (g_command_mutex && xSemaphoreTake(g_command_mutex, 0) != pdTRUE) {
        ESP_LOGW(TAG, "⚠️ Commande ignorée: Une autre commande est en cours de traitement");
        return;
    }
    
    ESP_LOGI(TAG, "🎯 Traitement commande type=%d", cmd->type);
    //print_command_details(cmd);
    
    // Apply the command based on its type
    switch (cmd->type) {
        case COMMAND_TYPE_LED:
            ESP_LOGI(TAG, "🎨 Processing LED command");
            esp_err_t ret = led_apply_command(cmd);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to apply LED command: %s", esp_err_to_name(ret));
            }
            break;
            
        case COMMAND_TYPE_HEATER:
            ESP_LOGI(TAG, "🔥 Processing heater command");
            // TODO: Implement heater command handler
            ESP_LOGW(TAG, "Heater command handler not yet implemented");
            break;
            
        case COMMAND_TYPE_HOOD:
            ESP_LOGI(TAG, "💨 Processing hood command");
            // TODO: Implement hood command handler
            ESP_LOGW(TAG, "Hood command handler not yet implemented");
            break;
            
        case COMMAND_TYPE_WATER_CASE:
            ESP_LOGI(TAG, "💧 Processing water case command");
            // TODO: Implement water case command handler
            ESP_LOGW(TAG, "Water case command handler not yet implemented");
            break;
            
        case COMMAND_TYPE_MULTIMEDIA:
            ESP_LOGI(TAG, "🎵 Processing multimedia command");
            esp_err_t proj_ret = videoprojecteur_apply_command(&cmd->command.videoprojecteur_cmd);
            if (proj_ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to apply projector command: %s", esp_err_to_name(proj_ret));
            }
            break;
            
            
        default:
            ESP_LOGW(TAG, "Unknown command type: %d", cmd->type);
            break;
    }
    
    // Libérer le mutex après traitement
    if (g_command_mutex) {
        xSemaphoreGive(g_command_mutex);
    }
}
// Un gestionnaire de fragments par connexion BLE (max 4 connexions)
#define MAX_BLE_CONNECTIONS 4
static fragment_handler_t g_fragment_handlers[MAX_BLE_CONNECTIONS];
static bool g_handlers_initialized = false;

void on_receive(uint16_t conn_handle, const uint8_t* data, size_t len) {
    // Initialiser les handlers au premier appel
    if (!g_handlers_initialized) {
        for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
            fragment_handler_init(&g_fragment_handlers[i], 5000); // 5s timeout
        }
        g_handlers_initialized = true;
    }
    
    // Trouver l'index du handler pour cette connexion (conn_handle % MAX)
    int handler_idx = conn_handle % MAX_BLE_CONNECTIONS;
    fragment_handler_t* handler = &g_fragment_handlers[handler_idx];
    
    ESP_LOGI(TAG, "📱 Data received from conn_handle=%d (handler_idx=%d) (%d bytes)", 
             conn_handle, handler_idx, len);
    
    // Afficher le début des données en hex (pour debug)
    if (len > 0) {
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, len < 32 ? len : 32, ESP_LOG_INFO);
    }
    
    // === ÉTAPE 1: Traiter la fragmentation ===
    uint8_t* complete_data = NULL;
    size_t complete_len = 0;
    
    fragment_result_t result = fragment_handler_process(
        handler,
        data,
        len,
        &complete_data,
        &complete_len
    );
    
    switch (result) {
        case FRAGMENT_RESULT_COMPLETE:
            // Données complètes disponibles, on peut parser la commande
            ESP_LOGI(TAG, "✅ Données complètes prêtes (%d bytes)", complete_len);
            
            // === ÉTAPE 2: Parser la commande VAN ===
            van_command_t* cmd = NULL;
            command_parse_result_t parse_result = parse_van_command(
                complete_data,
                complete_len,
                &cmd
            );
            
            if (parse_result == PARSE_SUCCESS) {
                ESP_LOGI(TAG, "✅ Commande parsée: type=%d", cmd->type);
                
                // === ÉTAPE 3: Traiter la commande ===
                handle_van_command(cmd);  // Votre fonction de traitement
                
                // Libérer la mémoire de la commande
                free_van_command(cmd);
            } else {
                ESP_LOGE(TAG, "❌ Échec parsing: %s", parse_result_to_string(parse_result));
            }
            
            // Libérer le buffer de fragmentation si nécessaire
            if (!handler->assembly.active && handler->assembly.buffer != NULL) {
                fragment_handler_cleanup(handler);
            }
            break;
            
        case FRAGMENT_RESULT_INCOMPLETE:
            ESP_LOGD(TAG, "⏳ Fragment reçu, attente des suivants...");
            break;
            
        case FRAGMENT_RESULT_ERROR_MEMORY:
            ESP_LOGE(TAG, "❌ Erreur mémoire lors du réassemblage (conn_handle=%d)", conn_handle);
            fragment_handler_cleanup(handler);
            break;
            
        case FRAGMENT_RESULT_ERROR_INVALID:
            ESP_LOGE(TAG, "❌ Fragment invalide (conn_handle=%d)", conn_handle);
            fragment_handler_cleanup(handler);
            break;
            
        case FRAGMENT_RESULT_ERROR_TIMEOUT:
            ESP_LOGE(TAG, "❌ Timeout réassemblage (conn_handle=%d)", conn_handle);
            break;
    }
}



esp_err_t update_van_state(void) {

    van_state_t* van_state = protocol_get_van_state();
    if (van_state) {
        #if ENABLE_ENERGY_SIMULATION
            // STEP 0: Update shared simulation time (MUST BE FIRST)
            energy_simulation_update_time();
        #endif
        
        // Update uptime
        protocol_update_uptime();
        
        // STEP 1: Update MPPT data (calculates solar current, stores in shared context)
        mppt_manager_update_van_state(van_state);
        
        #if ENABLE_ENERGY_SIMULATION
            // STEP 2: Update inverter/chargers (calculates net current, stores in shared context)
            inverter_chargers_manager_update_van_state(van_state);
        #endif
        
        // STEP 3: Update battery data (integrates net current → voltage/SOC, MUST BE LAST)
        battery_manager_update_van_state(van_state);
        // Update sensor data
        // TODO
        van_state->sensors.door_open = get_door_state(); // Update door state mannually since no manager yet
        // Update heater data
        heater_manager_update_van_state(van_state);
        // Update LED data
        led_manager_update_van_state(van_state);
        // Update video projector data
        videoprojecteur_manager_update_van_state(van_state);
        // Update HCO2T sensor data
        htco2_sensor_manager_update_van_state(van_state);

        // Finally, print the updated state for debugging
        #if PRINT_DEBUG_VAN_STATE
            protocol_print_state_summary();
        #endif

    } else {
        ESP_LOGE(TAG, "Failed to get van state for update");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Van state updated successfully");
    return ESP_OK;
}



void app_main(void) {
    ESP_LOGI(TAG, "MainPCB Van Controller starting...");
    
    // Créer le mutex pour gérer les commandes simultanées
    g_command_mutex = xSemaphoreCreateMutex();
    if (!g_command_mutex) {
        ESP_LOGE(TAG, "Failed to create command mutex!");
        return;
    }
    ESP_LOGI(TAG, "✅ Command mutex created (for multi-app command handling)");
    
    // Reduce NimBLE verbose logging (only show warnings/errors)
    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ///// Communication Protocol & BLE (INIT FIRST to avoid cache conflicts) ///
    ESP_LOGI(TAG, "Initializing protocol...");
    ESP_ERROR_CHECK(protocol_init());

    ESP_LOGI(TAG, "Initializing LED manager...");
    ESP_ERROR_CHECK(led_manager_init());
    
    // BLE Peripheral for mobile app (CPU @ 240MHz to reduce LED interference)
    ESP_LOGI(TAG, "Initializing BLE manager...");
    ble_init(on_receive);
    
    // Initialize all managers in order
    ESP_LOGI(TAG, "Initializing global coordinator...");
    ESP_ERROR_CHECK(global_coordinator_init());

    ESP_LOGI(TAG, "Initializing led coordinator...");
    ESP_ERROR_CHECK(led_coordinator_init());

    ESP_LOGI(TAG, "Initializing UART multiplexer...");
    ESP_ERROR_CHECK(uart_multiplexer_init());

    ESP_LOGI(TAG, "Initializing HCO2T sensor manager...");
    ESP_ERROR_CHECK(htco2_sensor_manager_init());

    ESP_LOGI(TAG, "Initializing switch manager...");
    ESP_ERROR_CHECK(switch_manager_init());

    ESP_LOGI(TAG, "Initializing hood control...");
    ESP_ERROR_CHECK(hood_init());

    ESP_LOGI(TAG, "Initializing heater manager...");
    ESP_ERROR_CHECK(heater_manager_init());

    ESP_LOGI(TAG, "Initializing video projector manager...");
    ESP_ERROR_CHECK(videoprojecteur_manager_init());

#if ENABLE_ENERGY_SIMULATION
    ESP_LOGI(TAG, "Initializing energy simulation context...");
    energy_simulation_init();
    
    ESP_LOGI(TAG, "Initializing inverter/chargers manager...");
    ESP_ERROR_CHECK(inverter_chargers_manager_init());
#endif
// Provisioirement commenter pour test car j'ai pas la batterie
    // ESP_LOGI(TAG, "Initializing battery manager...");
    // ESP_ERROR_CHECK(battery_manager_init());
// Provisioirement commenter pour test car j'ai pas la slave pcb
    // ESP_LOGI(TAG, "Initializing communication between main and slave manager...");
    // ESP_ERROR_CHECK(slave_main_communication_manager_init());

    
    ESP_LOGI(TAG, "All managers initialized successfully!");
    ESP_LOGI(TAG, "MainPCB Van Controller is running...");
    
    
    // Main loop - Send van state via BLE & monitor battery device
    #if ENABLE_ENERGY_SIMULATION
        uint32_t loop_counter = 0;
    #endif
    
    while(1) {
        
        update_van_state();
        app_main_send_van_state_to_app();
        
        #if ENABLE_ENERGY_SIMULATION
            // Print energy simulation summary every 10 seconds
            loop_counter++;
            // if (loop_counter % 10 == 0) {
            //     energy_simulation_print_summary();
            // }
        #endif
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // Update every 0.5 second
    }
    
 
    // Main loop
    // while(1) {
    //     // Just sleep, all work is done in tasks and callbacks
    //     uint8_t fuel_level = heater_manager_get_fuel_level();
    //     ESP_LOGI(TAG, "Current fuel level: %d%%", fuel_level);
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    // Main loop
    // while (1) {
        
    //     // Monitor system health
    //     van_state_t *state = protocol_get_state();
    //     if (state) {
            
    //         ESP_LOGD(TAG, "System uptime: %lu seconds", state->system.uptime);
    //         ESP_LOGD(TAG, "Fuel level: %.1f%%", state->sensors.fuel_level);
    //         ESP_LOGD(TAG, "Cabin temperature: %.1f°C", state->sensors.cabin_temperature);
    //         ESP_LOGD(TAG, "Heater water temp: %.1f°C", state->heater.water_temperature);
    //         ESP_LOGD(TAG, "Solar power: %.1fW + %.1fW", 
    //                 state->mppt.solar_power_100_50, state->mppt.solar_power_70_15);
            
            
    //         // Check for critical errors
    //         if (state->system.system_error) {
    //             ESP_LOGW(TAG, "System error detected:");
                
    //             // Decode individual errors
    //             if (state->system.error_code & ERROR_HEATER_NO_FUEL) {
    //                 ESP_LOGW(TAG, "  - No fuel for heater");
    //             }
    //             if (state->system.error_code & ERROR_MPPT_COMM) {
    //                 ESP_LOGW(TAG, "  - MPPT communication error");
    //             }
    //             if (state->system.error_code & ERROR_SENSOR_COMM) {
    //                 ESP_LOGW(TAG, "  - Sensor communication error");
    //             }
    //             if (state->system.error_code & ERROR_SLAVE_COMM) {
    //                 ESP_LOGW(TAG, "  - Slave PCB communication error");
    //             }
    //             if (state->system.error_code & ERROR_LED_STRIP) {
    //                 ESP_LOGW(TAG, "  - LED strip error");
    //             }
    //             if (state->system.error_code & ERROR_FAN_CONTROL) {
    //                 ESP_LOGW(TAG, "  - Fan control error");
    //             }
                
    //             led_trigger_error_mode();
    //         } else {
    //             // Periodically check if error conditions are resolved
    //             static uint32_t last_error_check = 0;
    //             uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                
    //             if (current_time - last_error_check > 30000) { // Check every 30 seconds
    //                 last_error_check = current_time;
                    
    //                 // Check fuel level for heater error
    //                 if (state->sensors.fuel_level > 5.0) { // If fuel level is above 5%
    //                     protocol_clear_error_flag(ERROR_HEATER_NO_FUEL);
    //                 }
                    
    //                 ESP_LOGD(TAG, "Periodic error condition check completed");
    //             }
    //         }
    //     }
        
    //     vTaskDelay(pdMS_TO_TICKS(10000)); // Update every 10 seconds
    // }
}
