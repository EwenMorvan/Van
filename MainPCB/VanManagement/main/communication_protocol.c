#include "communication_protocol.h"
#include "protocol.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#include "log_level.h"

static const char *TAG = "COMM_PROTOCOL";
static comm_interface_config_t interfaces[COMM_INTERFACE_MAX];
static TaskHandle_t protocol_task_handle;
static SemaphoreHandle_t protocol_mutex;

esp_err_t comm_protocol_init(void) {
    // Initialize interface configurations
    memset(interfaces, 0, sizeof(interfaces));
    
    // Set default intervals
    interfaces[COMM_INTERFACE_BLE].type = COMM_INTERFACE_BLE;
    interfaces[COMM_INTERFACE_BLE].state_interval_ms = 200; // 0.1 seconds

    interfaces[COMM_INTERFACE_USB].type = COMM_INTERFACE_USB;
    interfaces[COMM_INTERFACE_USB].state_interval_ms = 2000; // 2 seconds (faster for USB)
    
    // Create mutex
    protocol_mutex = xSemaphoreCreateMutex();
    if (protocol_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create protocol mutex");
        return ESP_FAIL;
    }
    
    // NOTE: Protocol task creation moved to comm_protocol_start() to avoid race conditions
    
    ESP_LOGI(TAG, "Communication protocol initialized");
    return ESP_OK;
}

esp_err_t comm_protocol_start(void) {
    if (protocol_mutex == NULL) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (protocol_task_handle != NULL) {
        ESP_LOGW(TAG, "Protocol task already running");
        return ESP_OK;
    }
    
    // Create protocol task on CPU1 to balance load with BLE on CPU0
    BaseType_t result = xTaskCreatePinnedToCore(
        comm_protocol_task,
        "comm_protocol",
        8192,  // Increased stack size for JSON operations
        NULL,
        3,
        &protocol_task_handle,
        1  // Pin to CPU1
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create communication protocol task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Communication protocol task started");
    return ESP_OK;
}

esp_err_t comm_protocol_register_interface(comm_interface_t interface, send_data_func_t send_func) {
    if (interface >= COMM_INTERFACE_MAX || send_func == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (protocol_mutex == NULL) {
        ESP_LOGW(TAG, "Protocol mutex not initialized, cannot register interface");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(protocol_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        interfaces[interface].send_func = send_func;
        ESP_LOGI(TAG, "Registered interface %d", interface);
        xSemaphoreGive(protocol_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

esp_err_t comm_protocol_set_connected(comm_interface_t interface, bool connected) {
    if (interface >= COMM_INTERFACE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (protocol_mutex == NULL) {
        ESP_LOGW(TAG, "Protocol mutex not initialized, cannot set connection status");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(protocol_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        interfaces[interface].is_connected = connected;
        ESP_LOGI(TAG, "Interface %d %s", interface, connected ? "connected" : "disconnected");
        xSemaphoreGive(protocol_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

esp_err_t comm_serialize_state(van_state_t *state, char *json_buffer, size_t buffer_size) {
    if (state == NULL || json_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON *type = cJSON_CreateString("state");
    cJSON *timestamp = cJSON_CreateNumber(xTaskGetTickCount() * portTICK_PERIOD_MS);
    cJSON *data = cJSON_CreateObject();
    
    // System data
    cJSON *system = cJSON_CreateObject();
    cJSON_AddNumberToObject(system, "uptime", state->system.uptime);
    cJSON_AddBoolToObject(system, "system_error", state->system.system_error);
    cJSON_AddNumberToObject(system, "error_code", state->system.error_code);
    cJSON_AddBoolToObject(system, "slave_pcb_connected", state->system.slave_pcb_connected);
    
    // Sensors data
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddNumberToObject(sensors, "fuel_level", state->sensors.fuel_level);
    cJSON_AddNumberToObject(sensors, "onboard_temperature", state->sensors.onboard_temperature);
    cJSON_AddNumberToObject(sensors, "cabin_temperature", state->sensors.cabin_temperature);
    cJSON_AddNumberToObject(sensors, "humidity", state->sensors.humidity);
    cJSON_AddNumberToObject(sensors, "co2_level", state->sensors.co2_level);
    cJSON_AddNumberToObject(sensors, "light_level", state->sensors.light_level);
    cJSON_AddBoolToObject(sensors, "van_light_active", state->sensors.van_light_active);
    // cJSON_AddBoolToObject(sensors, "door_open", state->sensors.door_open); // i think this is a consequence of van light
    
    // Heater data
    cJSON *heater = cJSON_CreateObject();
    cJSON_AddBoolToObject(heater, "heater_on", state->heater.heater_on);
    cJSON_AddNumberToObject(heater, "target_water_temp", state->heater.target_water_temp);
    cJSON_AddNumberToObject(heater, "target_cabin_temp", state->heater.target_cabin_temp);
    cJSON_AddNumberToObject(heater, "water_temperature", state->heater.water_temperature);
    cJSON_AddBoolToObject(heater, "pump_active", state->heater.pump_active);
    cJSON_AddNumberToObject(heater, "radiator_fan_speed", state->heater.radiator_fan_speed);
    
    // MPPT data
    cJSON *mppt = cJSON_CreateObject();
    cJSON_AddNumberToObject(mppt, "solar_power_100_50", state->mppt.solar_power_100_50);
    cJSON_AddNumberToObject(mppt, "battery_voltage_100_50", state->mppt.battery_voltage_100_50);
    cJSON_AddNumberToObject(mppt, "battery_current_100_50", state->mppt.battery_current_100_50);
    cJSON_AddNumberToObject(mppt, "temperature_100_50", state->mppt.temperature_100_50);
    cJSON_AddNumberToObject(mppt, "state_100_50", state->mppt.state_100_50);
    cJSON_AddNumberToObject(mppt, "solar_power_70_15", state->mppt.solar_power_70_15);
    cJSON_AddNumberToObject(mppt, "battery_voltage_70_15", state->mppt.battery_voltage_70_15);
    cJSON_AddNumberToObject(mppt, "battery_current_70_15", state->mppt.battery_current_70_15);
    cJSON_AddNumberToObject(mppt, "temperature_70_15", state->mppt.temperature_70_15);
    cJSON_AddNumberToObject(mppt, "state_70_15", state->mppt.state_70_15);
    
    // Fan data
    cJSON *fans = cJSON_CreateObject();
    cJSON_AddNumberToObject(fans, "elec_box_speed", state->fans.elec_box_speed);
    cJSON_AddNumberToObject(fans, "heater_fan_speed", state->fans.heater_fan_speed);
    cJSON_AddBoolToObject(fans, "hood_fan_active", state->fans.hood_fan_active);
    
    // LED data
    cJSON *leds = cJSON_CreateObject();
    cJSON *roof = cJSON_CreateObject();
    cJSON_AddNumberToObject(roof, "current_mode", state->leds.roof.current_mode);
    cJSON_AddNumberToObject(roof, "brightness", state->leds.roof.brightness);
    cJSON_AddBoolToObject(roof, "enabled", state->leds.roof.enabled);
    cJSON_AddBoolToObject(roof, "switch_pressed", state->leds.roof.switch_pressed);
    cJSON_AddNumberToObject(roof, "last_switch_time", state->leds.roof.last_switch_time);
    
    cJSON *exterior = cJSON_CreateObject();
    cJSON_AddNumberToObject(exterior, "current_mode", state->leds.exterior.current_mode);
    cJSON_AddNumberToObject(exterior, "brightness", state->leds.exterior.brightness);
    cJSON_AddBoolToObject(exterior, "power_enabled", state->leds.exterior.power_enabled);
    
    cJSON_AddItemToObject(leds, "roof", roof);
    cJSON_AddItemToObject(leds, "exterior", exterior);
    cJSON_AddBoolToObject(leds, "error_mode_active", state->leds.error_mode_active);
    
    // Add all sections to data
    cJSON_AddItemToObject(data, "system", system);
    cJSON_AddItemToObject(data, "sensors", sensors);
    cJSON_AddItemToObject(data, "heater", heater);
    cJSON_AddItemToObject(data, "mppt", mppt);
    cJSON_AddItemToObject(data, "fans", fans);
    cJSON_AddItemToObject(data, "leds", leds);
    
    // Add to root
    cJSON_AddItemToObject(root, "type", type);
    cJSON_AddItemToObject(root, "timestamp", timestamp);
    cJSON_AddItemToObject(root, "data", data);
    
    // Convert to string WITHOUT formatting to reduce size
    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    
    size_t json_length = strlen(json_string);
    if (json_length >= buffer_size) {
        free(json_string);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }
    
    strcpy(json_buffer, json_string);
    free(json_string);
    cJSON_Delete(root);
    
    return ESP_OK;
}

esp_err_t comm_serialize_response(const char* status, const char* message, char *json_buffer, size_t buffer_size) {
    if (status == NULL || json_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON *type = cJSON_CreateString("response");
    cJSON *status_obj = cJSON_CreateString(status);
    cJSON *timestamp = cJSON_CreateNumber(xTaskGetTickCount() * portTICK_PERIOD_MS);
    
    cJSON_AddItemToObject(root, "type", type);
    cJSON_AddItemToObject(root, "status", status_obj);
    cJSON_AddItemToObject(root, "timestamp", timestamp);
    
    if (message != NULL) {
        cJSON *message_obj = cJSON_CreateString(message);
        cJSON_AddItemToObject(root, "message", message_obj);
    }
    
    char *json_string = cJSON_Print(root);
    if (json_string == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    
    size_t json_length = strlen(json_string);
    if (json_length >= buffer_size) {
        free(json_string);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }
    
    strcpy(json_buffer, json_string);
    free(json_string);
    cJSON_Delete(root);
    
    return ESP_OK;
}

esp_err_t comm_parse_command(const char* json_data, van_command_t *command) {
    if (json_data == NULL || command == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *root = cJSON_Parse(json_data);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON command");
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "command") != 0) {
        ESP_LOGE(TAG, "Invalid message type");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (!cJSON_IsString(cmd)) {
        ESP_LOGE(TAG, "Missing command field");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Parse command type
    if (strcmp(cmd->valuestring, "set_heater_target") == 0) {
        command->type = CMD_SET_HEATER_TARGET;
    } else if (strcmp(cmd->valuestring, "set_heater_state") == 0) {
        command->type = CMD_SET_HEATER_STATE;
    } else if (strcmp(cmd->valuestring, "set_led_state") == 0) {
        command->type = CMD_SET_LED_STATE;
    } else if (strcmp(cmd->valuestring, "set_led_mode") == 0) {
        command->type = CMD_SET_LED_MODE;
    } else if (strcmp(cmd->valuestring, "set_led_brightness") == 0) {
        command->type = CMD_SET_LED_BRIGHTNESS;
    } else if (strcmp(cmd->valuestring, "set_hood_state") == 0) {
        command->type = CMD_SET_HOOD_STATE;
    } else {
        ESP_LOGE(TAG, "Unknown command: %s", cmd->valuestring);
        cJSON_Delete(root);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Parse target and value
    cJSON *target = cJSON_GetObjectItem(root, "target");
    cJSON *value = cJSON_GetObjectItem(root, "value");
    
    command->target = cJSON_IsNumber(target) ? target->valueint : 0;
    command->value = cJSON_IsNumber(value) ? value->valueint : 0;
    
    ESP_LOGI(TAG, "Parsed command: %s, target: %d, value: %d", 
             cmd->valuestring, command->target, command->value);
    
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t comm_protocol_process_received_data(comm_interface_t interface, const char* data, size_t length) {
    if (interface >= COMM_INTERFACE_MAX || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Received data from interface %d: %.*s", interface, (int)length, data);
    

    // Vérifier et supprimer le préfixe "ESP32:"
    const char *json_start = data;
    if (strncmp(data, "ESP32:", 6) == 0) {
        json_start = data + 6; // Ignorer les 6 premiers caractères
    }
    
    van_command_t command;
    esp_err_t result = comm_parse_command(json_start, &command);

    if (result == ESP_OK) {
        // Process the command
        protocol_process_command(&command);
        
        // Send success response
        comm_send_response(interface, "ok", "Command executed");

        //Test to see the satate afeter a command is recived
        // van_state_t *state = protocol_get_state();
        // if (state == NULL) {
        //     return ESP_ERR_INVALID_STATE;
        // }
        // ESP_LOGE(TAG, "Current state: %s", state);

    } else {
        // Send error response
        comm_send_response(interface, "error", "Invalid command format");
    }
    
    return result;
}

esp_err_t comm_broadcast_state(void) {
    van_state_t *state = protocol_get_state();
    if (state == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char json_buffer[MAX_JSON_MESSAGE_SIZE];
    esp_err_t result = comm_serialize_state(state, json_buffer, sizeof(json_buffer));
    
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to serialize state");
        return result;
    }
    
    ESP_LOGI(TAG, "Broadcasting state: %d bytes", strlen(json_buffer));
    ESP_LOGI(TAG, "State JSON: %.*s", (int)strlen(json_buffer), json_buffer);
    
    if (protocol_mutex == NULL) {
        ESP_LOGW(TAG, "Protocol mutex not initialized, skipping state broadcast");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(protocol_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Send to all connected interfaces
        for (int i = 0; i < COMM_INTERFACE_MAX; i++) {
            if (interfaces[i].is_connected && interfaces[i].send_func != NULL) {
                ESP_LOGI(TAG, "Sending state to interface %d (BLE=%d, USB=%d)", i, COMM_INTERFACE_BLE, COMM_INTERFACE_USB);
                esp_err_t send_result = interfaces[i].send_func(json_buffer, strlen(json_buffer));
                if (send_result != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to send state to interface %d: %s", i, esp_err_to_name(send_result));
                } else {
                    ESP_LOGI(TAG, "Successfully sent state to interface %d", i);
                }
            } else {
                ESP_LOGD(TAG, "Interface %d not ready - connected:%d, send_func:%p", i, interfaces[i].is_connected, interfaces[i].send_func);
            }
        }
        xSemaphoreGive(protocol_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to take protocol mutex for state broadcast");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t comm_send_response(comm_interface_t interface, const char* status, const char* message) {
    if (interface >= COMM_INTERFACE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (protocol_mutex == NULL) {
        ESP_LOGW(TAG, "Protocol mutex not initialized, cannot send response");
        return ESP_ERR_INVALID_STATE;
    }

    char json_buffer[MAX_COMMAND_BUFFER_SIZE];
    esp_err_t result = comm_serialize_response(status, message, json_buffer, sizeof(json_buffer));
    
    if (result != ESP_OK) {
        return result;
    }
    
    if (xSemaphoreTake(protocol_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (interfaces[interface].is_connected && interfaces[interface].send_func != NULL) {
            result = interfaces[interface].send_func(json_buffer, strlen(json_buffer));
        } else {
            result = ESP_ERR_INVALID_STATE;
        }
        xSemaphoreGive(protocol_mutex);
    }
    
    return result;
}

void comm_protocol_task(void *parameters) {
    ESP_LOGI(TAG, "Communication protocol task started");
    
    // Wait a moment for all systems to be ready
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Wait a bit to ensure initialization is complete
    vTaskDelay(pdMS_TO_TICKS(100));
    
    while (1) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        if (protocol_mutex == NULL) {
            ESP_LOGW(TAG, "Protocol mutex not initialized, waiting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        if (xSemaphoreTake(protocol_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Check each interface for periodic state broadcasting
            for (int i = 0; i < COMM_INTERFACE_MAX; i++) {
                if (interfaces[i].is_connected && 
                    (current_time - interfaces[i].last_state_sent) >= interfaces[i].state_interval_ms) {
                    
                    ESP_LOGI(TAG, "Triggering periodic state broadcast for interface %d (interval: %d ms)", 
                             i, interfaces[i].state_interval_ms);
                    interfaces[i].last_state_sent = current_time;
                    xSemaphoreGive(protocol_mutex);
                    
                    // Broadcast state (will take mutex again internally)
                    comm_broadcast_state();
                    
                    // Re-acquire mutex for next iteration
                    if (xSemaphoreTake(protocol_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
                        break;
                    }
                }
            }
            xSemaphoreGive(protocol_mutex);
        }
        
        // Use the smallest interval for task delay, with a minimum of 10ms and maximum of 100ms
        uint32_t min_interval = 1000; // Start with 1 second default
        for (int i = 0; i < COMM_INTERFACE_MAX; i++) {
            if (interfaces[i].is_connected && interfaces[i].state_interval_ms > 0) {
                if (interfaces[i].state_interval_ms < min_interval) {
                    min_interval = interfaces[i].state_interval_ms;
                }
            }
        }
        
        // Limit the delay to reasonable bounds
        if (min_interval < 10) min_interval = 10;      // Minimum 10ms
        if (min_interval > 100) min_interval = 100;    // Maximum 100ms for responsiveness
        
        vTaskDelay(pdMS_TO_TICKS(min_interval));
    }
}
