#include "mppt_manager.h"
// #include "../communications/communication_manager.h"
#include "../communications/uart/uart_multiplexer.h"
#include "../common_includes/gpio_pinout.h"
#include "../common_includes/simulation_config.h"
#ifdef ENABLE_ENERGY_SIMULATION
#include "energy_simulation.h"
#endif
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#if defined(ENABLE_ENERGY_SIMULATION) && ENABLE_ENERGY_SIMULATION
#define SIMULATE_MPPT_DATA 1  // Set to 1 to simulate MPPT data for testing
#else
#define SIMULATE_MPPT_DATA 0
#endif

static const char *TAG = "MPPT_MGR";
static TaskHandle_t mppt_task_handle;

typedef struct {
    float solar_power;
    float battery_voltage;
    float battery_current;
    int8_t temperature;
    uint8_t state;
    bool data_valid;
} mppt_data_t;

static mppt_data_t mppt_100_50_data;
static mppt_data_t mppt_70_15_data;

esp_err_t mppt_manager_init(void) {
    // Initialize MPPT data structures
    memset(&mppt_100_50_data, 0, sizeof(mppt_data_t));
    memset(&mppt_70_15_data, 0, sizeof(mppt_data_t));
    
    // Create MPPT task on CPU1 for better load balancing
    BaseType_t result = xTaskCreatePinnedToCore(
        mppt_manager_task,
        "mppt_manager",
        4096,
        NULL,
        3,
        &mppt_task_handle,
        1  // CPU1
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MPPT task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "MPPT manager initialized with UART multiplexing");
    return ESP_OK;
}

static void parse_ve_direct_frame(const char *frame, mppt_data_t *data) {
    char *line = strtok((char*)frame, "\n\r");
    
    while (line != NULL) {
        if (strncmp(line, "V\t", 2) == 0) {
            // Battery voltage in mV
            data->battery_voltage = atof(line + 2) / 1000.0f;
        } else if (strncmp(line, "I\t", 2) == 0) {
            // Battery current in mA
            data->battery_current = atof(line + 2) / 1000.0f;
        } else if (strncmp(line, "PPV\t", 4) == 0) {
            // Panel power in W
            data->solar_power = atof(line + 4);
        } else if (strncmp(line, "CS\t", 3) == 0) {
            // Charger state
            data->state = (uint8_t)atoi(line + 3);
        } else if (strncmp(line, "T\t", 2) == 0) {
            // Temperature in Celsius
            data->temperature = (int8_t)atoi(line + 2);
        } else if (strncmp(line, "Checksum\t", 9) == 0) {
            // End of frame
            data->data_valid = true;
            break;
        }
        
        line = strtok(NULL, "\n\r");
    }
}

static void read_mppt_data(mppt_device_t device, mppt_data_t *data) {
    static char buffer[VE_DIRECT_FRAME_SIZE];
    static int buffer_pos = 0;
    
    // Switch to the desired MPPT device
    ESP_LOGD(TAG, "Switching to MPPT device %d", device);
    if (uart_mux_switch_mppt(device) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to switch to MPPT device %d", device);
        return;
    }
    
    uint8_t uart_data[64];
    int len = uart_mux_read_mppt(uart_data, sizeof(uart_data), 1000);
    
    ESP_LOGD(TAG, "MPPT %d: Read %d bytes from UART", device, len);
    
    if (len > 0) {
        // Log raw data for debugging
        ESP_LOG_BUFFER_HEXDUMP(TAG, uart_data, len, ESP_LOG_DEBUG);
        
        for (int i = 0; i < len; i++) {
            char c = uart_data[i];
            
            if (buffer_pos < VE_DIRECT_FRAME_SIZE - 1) {
                buffer[buffer_pos++] = c;
            }
            
            // Check for frame start
            if (c == ':' && buffer_pos > 1) {
                // Found start of new frame, reset buffer
                buffer[0] = ':';
                buffer_pos = 1;
            }
            
            // Check for checksum line (end of frame)
            if (strstr(buffer, "Checksum") != NULL) {
                buffer[buffer_pos] = '\0';
                parse_ve_direct_frame(buffer, data);
                buffer_pos = 0;
                ESP_LOGD(TAG, "MPPT %d data: Power=%.1fW, Voltage=%.2fV, Current=%.2fA, Temp=%d°C", 
                         device, data->solar_power, data->battery_voltage, data->battery_current, data->temperature);
                break;
            }
        }
    } else {
        ESP_LOGW(TAG, "No data received from MPPT %d", device);
        data->data_valid = false;
    }
}

void mppt_manager_task(void *parameters) {
    ESP_LOGI(TAG, "MPPT manager task started");
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        // Read data from both MPPTs using multiplexer
        read_mppt_data(MPPT_100_50, &mppt_100_50_data);
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between reads
        read_mppt_data(MPPT_70_15, &mppt_70_15_data);
        
        // Check for communication errors
        static uint32_t last_100_50_update = 0;
        static uint32_t last_70_15_update = 0;
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        if (mppt_100_50_data.data_valid) {
            last_100_50_update = current_time;
            mppt_100_50_data.data_valid = false; // Reset flag
        }
        
        if (mppt_70_15_data.data_valid) {
            last_70_15_update = current_time;
            mppt_70_15_data.data_valid = false; // Reset flag
        }
        
        // Check for communication timeout (30 seconds)
        if (current_time - last_100_50_update > 30000) {
            // uint32_t error = ERROR_MPPT_COMM;
            uint32_t error = 0;

            // comm_send_message(COMM_MSG_ERROR, &error, sizeof(uint32_t));
        }
        
        if (current_time - last_70_15_update > 30000) {
            // uint32_t error = ERROR_MPPT_COMM;
            uint32_t error = 0;

            // comm_send_message(COMM_MSG_ERROR, &error, sizeof(uint32_t));
        }
        
        // Prepare MPPT data for communication manager
        struct {
            float solar_power_100_50;
            float battery_voltage_100_50;
            float battery_current_100_50;
            int8_t temperature_100_50;
            uint8_t state_100_50;
            
            float solar_power_70_15;
            float battery_voltage_70_15;
            float battery_current_70_15;
            int8_t temperature_70_15;
            uint8_t state_70_15;
        } mppt_data = {
            .solar_power_100_50 = mppt_100_50_data.solar_power,
            .battery_voltage_100_50 = mppt_100_50_data.battery_voltage,
            .battery_current_100_50 = mppt_100_50_data.battery_current,
            .temperature_100_50 = mppt_100_50_data.temperature,
            .state_100_50 = mppt_100_50_data.state,
            
            .solar_power_70_15 = mppt_70_15_data.solar_power,
            .battery_voltage_70_15 = mppt_70_15_data.battery_voltage,
            .battery_current_70_15 = mppt_70_15_data.battery_current,
            .temperature_70_15 = mppt_70_15_data.temperature,
            .state_70_15 = mppt_70_15_data.state
        };
        
        // Send MPPT data to communication manager
        // comm_send_message(COMM_MSG_MPPT_UPDATE, &mppt_data, sizeof(mppt_data));
        
        ESP_LOGD(TAG, "MPPT 100|50: %.1fW, %.2fV, %.2fA, %d°C, State:%d", 
                mppt_100_50_data.solar_power, mppt_100_50_data.battery_voltage, 
                mppt_100_50_data.battery_current, mppt_100_50_data.temperature, mppt_100_50_data.state);
        
        ESP_LOGD(TAG, "MPPT 70|15: %.1fW, %.2fV, %.2fA, %d°C, State:%d", 
                mppt_70_15_data.solar_power, mppt_70_15_data.battery_voltage, 
                mppt_70_15_data.battery_current, mppt_70_15_data.temperature, mppt_70_15_data.state);
        
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(MPPT_UPDATE_INTERVAL_MS));
    }
}

esp_err_t mppt_manager_update_van_state(van_state_t* van_state) {
    if (!van_state) {
        return ESP_ERR_INVALID_ARG;
    }
    
#ifdef SIMULATE_MPPT_DATA
    // ========== SIMULATION DES DONNÉES MPPT ==========
    // Use shared simulation context for coherent time and day cycle
    energy_simulation_context_t* sim_ctx = energy_simulation_get_context();
    
    // Tension batterie commune (lire depuis simulation context)
    float battery_voltage = sim_ctx->battery_voltage_v;
    if (battery_voltage < 10.0f || battery_voltage > 16.0f) {
        battery_voltage = 12.8f;  // Valeur par défaut si batterie pas encore initialisée
    }
    
    // Cycle jour/nuit depuis contexte partagé
    float day_cycle = sim_ctx->day_cycle;
    
    // ===== MPPT 100|50 (4 panneaux 130W en 2s2p = 520W max, 48V nominal) =====
    // Tension panneau: 48V nominal (2 séries de 24V), varie avec l'ensoleillement
    float panel_voltage_100_50 = 48.0f * (0.7f + 0.3f * day_cycle) + sinf(sim_ctx->time_ticks * 0.05f) * 2.0f;
    
    // Courant panneau: dépend de l'ensoleillement (max ~11A = 520W / 48V)
    float panel_current_100_50 = 11.0f * day_cycle * (0.9f + 0.1f * sinf(sim_ctx->time_ticks * 0.08f));
    
    // Puissance solaire
    float solar_power_100_50 = panel_voltage_100_50 * panel_current_100_50;
    
    // Courant batterie (avec efficacité MPPT ~96%)
    float battery_current_100_50 = (solar_power_100_50 * 0.96f) / battery_voltage;
    
    // Température qui augmente avec la puissance
    float temp_100_50 = 25.0f + (solar_power_100_50 / 50.0f) + sinf(sim_ctx->time_ticks * 0.03f) * 3.0f;
    
    // État du chargeur (0=Off, 3=Bulk, 4=Absorption, 5=Float)
    uint8_t state_100_50;
    if (solar_power_100_50 < 10.0f) {
        state_100_50 = 0;  // Off (nuit)
    } else if (battery_voltage < 13.5f) {
        state_100_50 = 3;  // Bulk (charge rapide)
    } else if (battery_voltage < 14.2f) {
        state_100_50 = 4;  // Absorption
    } else {
        state_100_50 = 5;  // Float (maintien)
    }
    
    // Mise à jour van_state
    van_state->mppt.solar_power_100_50 = solar_power_100_50;
    van_state->mppt.panel_voltage_100_50 = panel_voltage_100_50;
    van_state->mppt.panel_current_100_50 = panel_current_100_50;
    van_state->mppt.battery_voltage_100_50 = battery_voltage;
    van_state->mppt.battery_current_100_50 = battery_current_100_50;
    van_state->mppt.temperature_100_50 = (int8_t)temp_100_50;
    van_state->mppt.state_100_50 = state_100_50;
    van_state->mppt.error_flags_100_50 = 0;
    
    // ===== MPPT 70|15 (2 panneaux 100W en série = 200W max, 48V nominal) =====
    // Tension panneau: 48V nominal (2x24V en série)
    float panel_voltage_70_15 = 48.0f * (0.7f + 0.3f * day_cycle) + sinf(sim_ctx->time_ticks * 0.07f + 1.0f) * 1.5f;
    
    // Courant panneau: max ~4.2A = 200W / 48V
    float panel_current_70_15 = 4.2f * day_cycle * (0.85f + 0.15f * sinf(sim_ctx->time_ticks * 0.09f + 0.5f));
    
    // Puissance solaire
    float solar_power_70_15 = panel_voltage_70_15 * panel_current_70_15;
    
    // Courant batterie (avec efficacité MPPT ~96%)
    float battery_current_70_15 = (solar_power_70_15 * 0.96f) / battery_voltage;
    
    // Température légèrement différente
    float temp_70_15 = 23.0f + (solar_power_70_15 / 30.0f) + sinf(sim_ctx->time_ticks * 0.025f + 0.5f) * 3.0f;
    
    // État du chargeur
    uint8_t state_70_15;
    if (solar_power_70_15 < 10.0f) {
        state_70_15 = 0;  // Off
    } else if (battery_voltage < 13.5f) {
        state_70_15 = 3;  // Bulk
    } else if (battery_voltage < 14.2f) {
        state_70_15 = 4;  // Absorption
    } else {
        state_70_15 = 5;  // Float
    }
    
    // Mise à jour van_state
    van_state->mppt.solar_power_70_15 = solar_power_70_15;
    van_state->mppt.panel_voltage_70_15 = panel_voltage_70_15;
    van_state->mppt.panel_current_70_15 = panel_current_70_15;
    van_state->mppt.battery_voltage_70_15 = battery_voltage;
    van_state->mppt.battery_current_70_15 = battery_current_70_15;
    van_state->mppt.temperature_70_15 = (int8_t)temp_70_15;
    van_state->mppt.state_70_15 = state_70_15;
    van_state->mppt.error_flags_70_15 = 0;
    
    // Store total solar power and current in shared context
    sim_ctx->solar_current_a = battery_current_100_50 + battery_current_70_15;
    sim_ctx->solar_power_w = solar_power_100_50 + solar_power_70_15;
    
    // Log périodique (toutes les 10 secondes)
    if (sim_ctx->time_ticks % 500 == 0) {
        ESP_LOGI(TAG, "☀️ MPPT 100|50: %.1fW (Panel:%.1fV/%.2fA → Bat:%.2fV/%.2fA) %d°C State:%d",
                 solar_power_100_50,
                 panel_voltage_100_50, panel_current_100_50,
                 battery_voltage, battery_current_100_50,
                 (int)temp_100_50, state_100_50);
        
        ESP_LOGI(TAG, "☀️ MPPT 70|15: %.1fW (Panel:%.1fV/%.2fA → Bat:%.2fV/%.2fA) %d°C State:%d",
                 solar_power_70_15,
                 panel_voltage_70_15, panel_current_70_15,
                 battery_voltage, battery_current_70_15,
                 (int)temp_70_15, state_70_15);
    }
    
#else
    // ========== DONNÉES RÉELLES DES MPPT ==========
    // Utiliser les données lues par mppt_manager_task
    van_state->mppt.solar_power_100_50 = mppt_100_50_data.solar_power;
    van_state->mppt.panel_voltage_100_50 = 0.0f;  // TODO: extraire du VE.Direct
    van_state->mppt.panel_current_100_50 = 0.0f;  // TODO: extraire du VE.Direct
    van_state->mppt.battery_voltage_100_50 = mppt_100_50_data.battery_voltage;
    van_state->mppt.battery_current_100_50 = mppt_100_50_data.battery_current;
    van_state->mppt.temperature_100_50 = mppt_100_50_data.temperature;
    van_state->mppt.state_100_50 = mppt_100_50_data.state;
    van_state->mppt.error_flags_100_50 = 0;  // TODO: gérer les erreurs
    
    van_state->mppt.solar_power_70_15 = mppt_70_15_data.solar_power;
    van_state->mppt.panel_voltage_70_15 = 0.0f;  // TODO: extraire du VE.Direct
    van_state->mppt.panel_current_70_15 = 0.0f;  // TODO: extraire du VE.Direct
    van_state->mppt.battery_voltage_70_15 = mppt_70_15_data.battery_voltage;
    van_state->mppt.battery_current_70_15 = mppt_70_15_data.battery_current;
    van_state->mppt.temperature_70_15 = mppt_70_15_data.temperature;
    van_state->mppt.state_70_15 = mppt_70_15_data.state;
    van_state->mppt.error_flags_70_15 = 0;  // TODO: gérer les erreurs
#endif
    
    return ESP_OK;
}
