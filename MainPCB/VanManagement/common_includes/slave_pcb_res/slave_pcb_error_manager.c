#include "slave_pcb_error_manager.h"

static const char *TAG = "SLAVE_ERROR_MGR";

// Obtenir une description textuelle pour chaque code d'erreur
// static const char* get_error_string(uint32_t error_code) {
//     switch(error_code) {
//         // Initialization errors (0x1XXX)
//         case 0x1001: return "Invalid argument";
//         case 0x1002: return "Initialization failed";
//         case 0x1003: return "Memory error";
        
//         // Communication errors (0x2XXX)
//         case 0x2001: return "Communication failure";
//         case 0x2002: return "I2C failure";
//         case 0x2003: return "SPI failure";
//         case 0x2004: return "Operation timeout";
//         case 0x2005: return "Ethernet disconnected";
        
//         // Device errors (0x3XXX)
//         case 0x3001: return "Device not found";
//         case 0x3002: return "Device busy";
//         case 0x3003: return "Device fault";
        
//         // State/Case errors (0x4XXX)
//         case 0x4001: return "Invalid state";
//         case 0x4002: return "Incompatible case";
//         case 0x4003: return "Invalid case transition";
        
//         // Safety errors (0x5XXX)
//         case 0x5001: return "Safety limit reached";
//         case 0x5002: return "Emergency stop";
//         case 0x5003: return "Overcurrent detected";
//         case 0x5004: return "Sensor out of range";
        
//         default: return "Unknown error";
//     }
// }

void print_slave_error_event(const slave_error_event_t* event) {
    if (!event || event->error_code == 0) return;

    // Définir les couleurs pour chaque niveau de sévérité
    const char* severity_str[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
    const char* severity_color[] = {
        "\033[0;32m",  // Vert pour INFO
        "\033[0;33m",  // Jaune pour WARNING
        "\033[0;31m",  // Rouge pour ERROR
        "\033[1;31m"   // Rouge vif pour CRITICAL
    };
    const char* reset_color = "\033[0m";

    // Afficher l'erreur avec la couleur appropriée
    ESP_LOGI(TAG, "    %s[%s]%s %s in %s: %s (0x%X)",
             severity_color[event->severity],
             severity_str[event->severity],
             reset_color,
             get_error_string(event->error_code),
             event->module,
             event->description,
             event->data);
}

void print_slave_error_stats(const slave_error_stats_t* stats) {
    if (!stats) return;

    ESP_LOGD(TAG, "Error Statistics:");
    ESP_LOGD(TAG, "  Total errors: %lu", stats->total_errors);

    // Afficher les erreurs par sévérité
    if (stats->total_errors > 0) {
        const char* severity_names[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
        ESP_LOGD(TAG, "  By severity:");
        for (int i = 0; i < 4; i++) {
            if (stats->errors_by_severity[i] > 0) {
                ESP_LOGD(TAG, "    %s: %lu", severity_names[i], stats->errors_by_severity[i]);
            }
        }

        // Afficher les erreurs par catégorie
        const char* category_names[] = {
            "None", "Init", "Comm", "Device", 
            "Sensor", "Actuator", "System", "Safety"
        };
        ESP_LOGD(TAG, "  By category:");
        for (int i = 0; i < 8; i++) {
            if (stats->errors_by_category[i] > 0) {
                ESP_LOGD(TAG, "    %s: %lu", category_names[i], stats->errors_by_category[i]);
            }
        }
    }
}

void print_slave_error_state(const slave_error_state_t* state) {
    if (!state) return;

    ESP_LOGD(TAG, "=== Slave PCB Error State ===");
    
    // Imprimer les statistiques globales
    print_slave_error_stats(&state->error_stats);
    
    // Imprimer l'historique des erreurs
    if (state->error_stats.total_errors > 0) {
        ESP_LOGD(TAG, "Error History:");
        for (int i = 0; i < MAX_ERROR_HISTORY; i++) {
            const slave_error_event_t* event = &state->last_errors[i];
            if (event->error_code != 0) {
                ESP_LOGD(TAG, "  [%d]:", i + 1);
                print_slave_error_event(event);
            }
        }
    }
    
    ESP_LOGD(TAG, "=========================");
}