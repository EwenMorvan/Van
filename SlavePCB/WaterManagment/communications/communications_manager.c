#include "communications_manager.h"
#include <string.h>
#include "esp_system.h"
#include "esp_heap_caps.h"

static const char *TAG = "COMM_MGR";

// Global variables
static slave_pcb_state_t current_state = {0};
static SemaphoreHandle_t state_mutex = NULL;

// Implémentation de l'API d'accès à l'état
slave_pcb_state_t* get_system_state(void) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        slave_pcb_state_t* state = &current_state;
        xSemaphoreGive(state_mutex);
        return state;
    }
    return NULL;
}

void update_system_case(system_case_t new_case) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        current_state.current_case = new_case;
        current_state.timestamp = esp_timer_get_time() / 1000;
        xSemaphoreGive(state_mutex);
    }
}

void update_hood_state(hood_state_t new_state) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        current_state.hood_state = new_state;
        current_state.timestamp = esp_timer_get_time() / 1000;
        xSemaphoreGive(state_mutex);
    }
}

void update_tank_levels(const water_tanks_levels_t* new_levels) {
    if (new_levels && xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&current_state.tanks_levels, new_levels, sizeof(water_tanks_levels_t));
        current_state.timestamp = esp_timer_get_time() / 1000;
        xSemaphoreGive(state_mutex);
    }
}

void update_system_health(const slave_health_t* new_health) {
    if (new_health && xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&current_state.system_health, new_health, sizeof(slave_health_t));
        current_state.timestamp = esp_timer_get_time() / 1000;
        xSemaphoreGive(state_mutex);
    }
}

static void update_system_health_metrics(void) {
    slave_health_t health = {
        .system_healthy = true,  // À mettre à false si des erreurs critiques sont détectées
        .last_health_check = esp_timer_get_time() / 1000,
        .uptime_seconds = esp_timer_get_time() / 1000000,  // Convertir µs en secondes
        .free_heap_size = esp_get_free_heap_size(),
        .min_free_heap_size = esp_get_minimum_free_heap_size()
    };
    
    // Si la mémoire disponible est trop basse, marquer le système comme non-healthy
    if (health.free_heap_size < 4096) {  // Exemple de seuil
        health.system_healthy = false;
    }
    
    update_system_health(&health);
}

void update_error_state(const slave_error_state_t* new_error_state) {
    if (new_error_state && xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&current_state.error_state, new_error_state, sizeof(slave_error_state_t));
        current_state.timestamp = esp_timer_get_time() / 1000;
        xSemaphoreGive(state_mutex);
    }
}

// Private variables
static uint16_t sequence_counter = 0;
static SemaphoreHandle_t comm_mutex = NULL;
static QueueHandle_t ack_queue = NULL;
static TaskHandle_t _state_update_task_handle = NULL;
static comm_command_callback_t command_callback = NULL;
static slave_pcb_err_t last_error = SLAVE_PCB_OK;

// Private functions declarations
static void _ethernet_receive_cb(const uint8_t *data, uint32_t length, const char *source_ip, uint16_t source_port);
static void _state_update_task(void *pvParameters);
static esp_err_t _wait_for_ack(uint16_t sequence, uint32_t timeout_ms);
static void _handle_command_message(const comm_command_msg_t *cmd_msg);
static void _handle_response_message(const comm_response_msg_t *resp_msg);
static uint16_t _get_next_sequence(void);


static uint16_t _get_next_sequence(void) {
    return ++sequence_counter;
}

static void _ethernet_receive_cb(const uint8_t *data, uint32_t length, const char *source_ip, uint16_t source_port) {
    if (!data || length < sizeof(comm_msg_header_t)) {
        ESP_LOGW(TAG, "Received message too short for header (%u bytes)", (unsigned)length);
        return;
    }

    const comm_msg_header_t *header = (const comm_msg_header_t *)data;
    ESP_LOGD(TAG, "Received message: type=0x%02x, seq=%u, len=%u", 
             header->type, header->sequence, header->length);

    // Vérifier que la longueur totale correspond
    size_t expected_length = sizeof(comm_msg_header_t) + header->length;
    if (length != expected_length) {
        ESP_LOGW(TAG, "Message length mismatch: got %u, expected %u bytes", 
                 (unsigned)length, (unsigned)expected_length);
        return;
    }

    switch (header->type) {
        case MSG_TYPE_COMMAND: {
            if (header->length >= sizeof(comm_command_msg_t) - sizeof(comm_msg_header_t)) {
                _handle_command_message((const comm_command_msg_t *)data);
            }
            break;
        }
        case MSG_TYPE_ACK:
        case MSG_TYPE_NACK: {
            if (header->length >= sizeof(comm_response_msg_t) - sizeof(comm_msg_header_t)) {
                _handle_response_message((const comm_response_msg_t *)data);
            }
            break;
        }
        case MSG_TYPE_STATE: {
            ESP_LOGW(TAG, "Unexpected state message from %s", source_ip);
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown message type: 0x%02x", header->type);
            break;
    }
}

static void _handle_command_message(const comm_command_msg_t *cmd_msg) {
    ESP_LOGI(TAG, "Received command: %d, sequence: %d", cmd_msg->command, cmd_msg->header.sequence);
    
    // Prepare response
    comm_response_msg_t response = {
        .header = {
            .type = MSG_TYPE_ACK,
            .sequence = _get_next_sequence(),
            .length = sizeof(comm_response_msg_t) - sizeof(comm_msg_header_t),
            .timestamp = esp_timer_get_time() / 1000
        },
        .command_sequence = cmd_msg->header.sequence,
        .status = SLAVE_PCB_OK
    };

    // Execute command if callback is registered
    if (command_callback) {
        command_callback(cmd_msg->command, cmd_msg->parameters, 
                       cmd_msg->header.length - sizeof(comm_command_msg_t) + sizeof(comm_msg_header_t));
    } else {
        response.header.type = MSG_TYPE_NACK;
        response.status = SLAVE_PCB_ERR_STATE_INVALID;
    }

    // Send response
    if (ethernet_send((const uint8_t *)&response, sizeof(response), MAIN_PCB_IP, MAIN_PCB_PORT) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send command response");
    }
}

static void _handle_response_message(const comm_response_msg_t *resp_msg) {
    if (ack_queue) {
        xQueueSend(ack_queue, resp_msg, 0);
    }
}

static esp_err_t _wait_for_ack(uint16_t sequence, uint32_t timeout_ms) {
    comm_response_msg_t response;
    if (xQueueReceive(ack_queue, &response, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        if (response.command_sequence == sequence) {
            return response.status == SLAVE_PCB_OK ? ESP_OK : ESP_FAIL;
        }
    }
    return ESP_ERR_TIMEOUT;
}

static void _state_update_task(void *pvParameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        // Mettre à jour les métriques système
        update_system_health_metrics();
        
        // Envoyer l'état complet
        communications_send_state();
        
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(COMM_STATE_UPDATE_PERIOD_MS));
    }
}

// Public functions
esp_err_t communications_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Communication Manager");
    
    // Créer le mutex pour protéger l'accès à current_state
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL) {
        REPORT_ERROR(SLAVE_PCB_ERR_MEMORY, TAG, "Failed to create state mutex", 0);
        return ESP_ERR_NO_MEM;
    }
    
    // Initialiser l'état par défaut
    memset(&current_state, 0, sizeof(current_state));
    current_state.timestamp = esp_timer_get_time() / 1000;

    // Create synchronization primitives
    comm_mutex = xSemaphoreCreateMutex();
    ack_queue = xQueueCreate(5, sizeof(comm_response_msg_t));
    
    if (!comm_mutex || !ack_queue) {
        REPORT_ERROR(SLAVE_PCB_ERR_MEMORY, TAG, "Failed to create synchronization primitives", 0);
        return ESP_ERR_NO_MEM;
    }

    // Initialize ethernet with client config
    esp_err_t ret = ethernet_manager_init(&ETHERNET_CLIENT_CONFIG);
    if (ret != ESP_OK) {
        REPORT_ERROR(SLAVE_PCB_ERR_INIT_FAIL, TAG, "Failed to initialize ethernet manager", ret);
        return ret;
    }

    // Set receive callback
    ethernet_set_receive_callback(_ethernet_receive_cb);
    // Start state update task
    ret = communications_start_state_update_task();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start state update task");
        return ret;
    }

    ESP_LOGI(TAG, "Communication Manager initialized successfully");
    return ESP_OK;
}

esp_err_t communications_send_command_with_ack(comm_cmd_t command, const uint8_t *params, 
                                             uint16_t params_len, uint32_t timeout_ms) {
    if (params_len > 32) {
        return ESP_ERR_INVALID_ARG;
    }

    comm_command_msg_t cmd_msg = {
        .header = {
            .type = MSG_TYPE_COMMAND,
            .sequence = _get_next_sequence(),
            .length = sizeof(comm_command_msg_t) - sizeof(comm_msg_header_t) + params_len,
            .timestamp = esp_timer_get_time() / 1000
        },
        .command = command
    };

    if (params && params_len > 0) {
        memcpy(cmd_msg.parameters, params, params_len);
    }

    for (int retry = 0; retry < COMM_MAX_RETRIES; retry++) {
        if (ethernet_send((const uint8_t *)&cmd_msg, sizeof(cmd_msg), MAIN_PCB_IP, MAIN_PCB_PORT) == ESP_OK) {
            if (_wait_for_ack(cmd_msg.header.sequence, timeout_ms) == ESP_OK) {
                return ESP_OK;
            }
        }
        ESP_LOGW(TAG, "Command retry %d/%d", retry + 1, COMM_MAX_RETRIES);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    last_error = SLAVE_PCB_ERR_COMM_FAIL;
    return ESP_FAIL;
}

esp_err_t communications_send_state(void) {
    slave_pcb_state_t* current_state = get_system_state();
    if (!current_state) {
        REPORT_ERROR(SLAVE_PCB_ERR_STATE_INVALID, TAG, "Failed to get system state", 0);
        return ESP_FAIL;
    }
    
    // Mise à jour complète de l'état des erreurs avant l'envoi
    slave_error_state_t* error_state = error_get_system_state();
    if (error_state) {
        // Copie l'état complet des erreurs
        memcpy(&current_state->error_state, error_state, sizeof(slave_error_state_t));
        
        // Log des erreurs importantes avant l'envoi
        ESP_LOGD(TAG, "Sending error state - Total: %d, Critical: %d, Recent: %d",
                error_state->error_stats.total_errors,
                error_state->error_stats.errors_by_severity[ERR_SEVERITY_CRITICAL],
                error_state->last_errors[0].error_code != SLAVE_PCB_OK ? 1 : 0);
        
        // Log des erreurs récentes
        for (int i = 0; i < 5 && error_state->last_errors[i].error_code != SLAVE_PCB_OK; i++) {
            const error_event_t* err = &error_state->last_errors[i];
            ESP_LOGD(TAG, "Recent error [%d]: %s in %s (Severity: %d, Data: 0x%X)", 
                    i, get_error_string(err->error_code), err->module, 
                    err->severity, err->data);
        }
    }
    
    // Log de l'état complet avec détails des erreurs
    ESP_LOGD(TAG, "Sending state: case=%d, hood=%d, tanks=[%d,%d], uptime=%d",
             current_state->current_case,
             current_state->hood_state,
             current_state->tanks_levels.tank_a.level_percentage,
             current_state->tanks_levels.tank_b.level_percentage,
             current_state->system_health.uptime_seconds);

    // Log détaillé des erreurs
    ESP_LOGD(TAG, "Error state details:");
    ESP_LOGD(TAG, "  Total errors: %d", current_state->error_state.error_stats.total_errors);

    // Afficher les 5 dernières erreurs
    ESP_LOGD(TAG, "  Last errors:");
    for (int i = 0; i < 5 && current_state->error_state.last_errors[i].error_code != SLAVE_PCB_OK; i++) {
        const error_event_t* err = &current_state->error_state.last_errors[i];
        ESP_LOGD(TAG, "    [%d] Code: 0x%X - %s",
                 i + 1,
                 err->error_code,
                 get_error_string(err->error_code));
        ESP_LOGD(TAG, "        Module: %s", err->module);
        ESP_LOGD(TAG, "        Description: %s", err->description);
        ESP_LOGD(TAG, "        Severity: %d, Time: %lu, Data: 0x%X",
                 err->severity,
                 err->timestamp,
                 err->data);
    }

    // Afficher les statistiques par sévérité
    ESP_LOGD(TAG, "  Errors by severity:");
    const char* severity_names[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
    for (int i = 0; i < 4; i++) {
        if (current_state->error_state.error_stats.errors_by_severity[i] > 0) {
            ESP_LOGD(TAG, "    %s: %d", 
                     severity_names[i],
                     current_state->error_state.error_stats.errors_by_severity[i]);
        }
    }

    // Afficher les statistiques par catégorie
    ESP_LOGD(TAG, "  Errors by category:");
    const char* category_names[] = {"None", "Init", "Comm", "Device", 
                                  "Sensor", "Actuator", "System", "Safety"};
    for (int i = 0; i < 8; i++) {
        if (current_state->error_state.error_stats.errors_by_category[i] > 0) {
            ESP_LOGD(TAG, "    %s: %d",
                     category_names[i],
                     current_state->error_state.error_stats.errors_by_category[i]);
        }
    }

    // Préparer le message d'état avec l'en-tête
    comm_state_msg_t state_msg = {
        .header = {
            .type = MSG_TYPE_STATE,
            .sequence = _get_next_sequence(),
            .length = sizeof(slave_pcb_state_t),
            .timestamp = esp_timer_get_time() / 1000
        }
    };
    
    // Copier l'état actuel
    memcpy(&state_msg.state, current_state, sizeof(slave_pcb_state_t));
    
    size_t total_size = sizeof(comm_state_msg_t);
    ESP_LOGD(TAG, "Sending state message: size=%u bytes, type=STATE, seq=%u", 
             (unsigned)total_size, state_msg.header.sequence);
             
    esp_err_t send_ret = ethernet_send((const uint8_t *)&state_msg, total_size, 
                                      MAIN_PCB_IP, MAIN_PCB_PORT);
    if (send_ret != ESP_OK) {
        ESP_LOGE(TAG, "ethernet_send failed: %d", send_ret);
    }
    return send_ret;
}


esp_err_t communications_start_state_update_task(void) {
    return xTaskCreate(_state_update_task, "state_update", 4096, NULL, 5, &_state_update_task_handle) == pdPASS ? 
           ESP_OK : ESP_FAIL;
}

slave_pcb_err_t communications_get_last_error(void) {
    return last_error;
}

esp_err_t communications_register_command_callback(comm_command_callback_t callback) {
    if (!callback) {
        return ESP_ERR_INVALID_ARG;
    }
    command_callback = callback;
    return ESP_OK;
}
