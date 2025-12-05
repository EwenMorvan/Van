
#include "command_parser.h"

static const char *TAG = "CMD_PARSER";
// ============================================================================
// PRIVATE FUNCTIONS DECLARATIONS
// ============================================================================

static bool parse_led_static_command(const uint8_t* data, size_t* offset, led_static_command_t* cmd, size_t data_len);
static bool parse_led_dynamic_command(const uint8_t* data, size_t* offset, led_dynamic_command_t** cmd, size_t data_len);
static bool parse_led_keyframe(const uint8_t* data, size_t* offset, led_keyframe_t* keyframe, led_strip_dynamic_target_t target, size_t data_len);
static bool validate_led_command(const led_command_t* cmd);

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

command_parse_result_t parse_van_command(const uint8_t* raw_data, size_t data_len, van_command_t** output_cmd) {
    if (raw_data == NULL || output_cmd == NULL || data_len < MIN_VAN_COMMAND_SIZE) {
        return PARSE_ERROR_INVALID_INPUT;
    }

    // Allocate base command structure
    *output_cmd = (van_command_t*)malloc(sizeof(van_command_t));
    if (*output_cmd == NULL) {
        return PARSE_ERROR_MEMORY;
    }
    memset(*output_cmd, 0, sizeof(van_command_t));

    size_t offset = 0;

    // Parse command type (1 byte)
    if (offset + sizeof(uint8_t) > data_len) {
        free(*output_cmd);
        *output_cmd = NULL;
        return PARSE_ERROR_INCOMPLETE_DATA;
    }
    (*output_cmd)->type = (command_type_t)raw_data[offset++];

    // Parse timestamp (4 bytes)
    if (offset + sizeof(uint32_t) > data_len) {
        free(*output_cmd);
        *output_cmd = NULL;
        return PARSE_ERROR_INCOMPLETE_DATA;
    }
    memcpy(&(*output_cmd)->timestamp, &raw_data[offset], sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Parse specific command data based on type
    switch ((*output_cmd)->type) {
        case COMMAND_TYPE_LED: {
            // Parse LED type (1 byte)
            if (offset + sizeof(uint8_t) > data_len) {
                free(*output_cmd);
                *output_cmd = NULL;
                return PARSE_ERROR_INCOMPLETE_DATA;
            }
            led_type_t led_type = (led_type_t)raw_data[offset++];
            (*output_cmd)->command.led_cmd.led_type = led_type;

            if (led_type == LED_STATIC) {
                if (!parse_led_static_command(raw_data, &offset, &(*output_cmd)->command.led_cmd.command.static_cmd, data_len)) {
                    free(*output_cmd);
                    *output_cmd = NULL;
                    return PARSE_ERROR_LED_DATA;
                }
            } else if (led_type == LED_DYNAMIC) {
                led_dynamic_command_t* dynamic_cmd = NULL;
                if (!parse_led_dynamic_command(raw_data, &offset, &dynamic_cmd, data_len)) {
                    free(*output_cmd);
                    *output_cmd = NULL;
                    ESP_LOGW(TAG, "Failed to parse LED dynamic command 1");
                    return PARSE_ERROR_LED_DATA;
                }
                (*output_cmd)->command.led_cmd.command.dynamic_cmd = dynamic_cmd;
            } else {
                free(*output_cmd);
                *output_cmd = NULL;
                ESP_LOGW(TAG, "Failed to parse LED dynamic command 2");
                return PARSE_ERROR_LED_DATA;
            }
            break;
        }

        case COMMAND_TYPE_HEATER: {
            if (offset + sizeof(heater_command_t) > data_len) {
                free(*output_cmd);
                *output_cmd = NULL;
                return PARSE_ERROR_INCOMPLETE_DATA;
            }
            memcpy(&(*output_cmd)->command.heater_cmd, &raw_data[offset], sizeof(heater_command_t));
            offset += sizeof(heater_command_t);
            break;
        }

        case COMMAND_TYPE_HOOD: {
            if (offset + sizeof(uint8_t) > data_len) {
                free(*output_cmd);
                *output_cmd = NULL;
                return PARSE_ERROR_INCOMPLETE_DATA;
            }
            (*output_cmd)->command.hood_cmd = (hood_command_t)raw_data[offset++];
            break;
        }

        case COMMAND_TYPE_WATER_CASE: {
            if (offset + sizeof(uint8_t) > data_len) {
                free(*output_cmd);
                *output_cmd = NULL;
                return PARSE_ERROR_INCOMPLETE_DATA;
            }
            (*output_cmd)->command.water_case_cmd.cmd_case_number = (system_case_t)raw_data[offset++];
            break;
        }

        case COMMAND_TYPE_MULTIMEDIA: {
            if (offset + sizeof(uint8_t) > data_len) {
                free(*output_cmd);
                *output_cmd = NULL;
                return PARSE_ERROR_INCOMPLETE_DATA;
            }
            (*output_cmd)->command.videoprojecteur_cmd.cmd = (projector_command_t)raw_data[offset++];
            break;
        }

        default:
            free(*output_cmd);
            *output_cmd = NULL;
            return PARSE_ERROR_UNKNOWN_TYPE;
    }

    // Validate the parsed command
    if (!validate_parsed_command(*output_cmd)) {
        free(*output_cmd);
        *output_cmd = NULL;
        return PARSE_ERROR_VALIDATION_FAILED;
    }

    return PARSE_SUCCESS;
}

// ============================================================================
// PRIVATE FUNCTIONS IMPLEMENTATIONS
// ============================================================================

static bool parse_led_static_command(const uint8_t* data, size_t* offset, led_static_command_t* cmd, size_t data_len) {
    if (*offset + sizeof(uint8_t) > data_len) return false;
    cmd->strip_target = (led_strip_static_target_t)data[(*offset)++];

    // Parse based on target type
    switch (cmd->strip_target) {
        case ROOF_LED1:
        case ROOF_LED2:
        case ROOF_LED_ALL: {
            size_t roof1_size = sizeof(led_roof1_strip_colors_t);
            size_t roof2_size = sizeof(led_roof2_strip_colors_t);
            
            if (*offset + roof1_size + roof2_size > data_len) return false;
            
            memcpy(&cmd->colors.roof.roof1_colors, &data[*offset], roof1_size);
            *offset += roof1_size;
            memcpy(&cmd->colors.roof.roof2_colors, &data[*offset], roof2_size);
            *offset += roof2_size;
            break;
        }

        case EXT_AV_LED:
        case EXT_AR_LED:
        case EXT_LED_ALL: {
            size_t ext_av_size = sizeof(led_ext_av_strip_colors_t);
            size_t ext_ar_size = sizeof(led_ext_ar_strip_colors_t);
            
            if (*offset + ext_av_size + ext_ar_size > data_len) return false;
            
            memcpy(&cmd->colors.ext.ext_av_colors, &data[*offset], ext_av_size);
            *offset += ext_av_size;
            memcpy(&cmd->colors.ext.ext_ar_colors, &data[*offset], ext_ar_size);
            *offset += ext_ar_size;
            break;
        }

        default:
            return false;
    }

    return true;
}

static bool parse_led_dynamic_command(const uint8_t* data, size_t* offset, led_dynamic_command_t** cmd, size_t data_len) {
    ESP_LOGI(TAG, "🔍 parse_led_dynamic START: offset=%zu, data_len=%zu", *offset, data_len);
    
    // Read fixed part first
     // Taille de la partie fixe: strip_target(1) + loop_duration(4) + keyframe_count(2) + loop_behavior(1) = 8 bytes
    size_t fixed_part_size = 1 + sizeof(uint32_t) + sizeof(uint16_t) + 1;
    size_t needed = *offset + fixed_part_size;
    ESP_LOGI(TAG, "🔍 Check space: need %zu bytes, have %zu", needed, data_len);
    if (needed > data_len) {
        ESP_LOGE(TAG, "❌ Not enough data for fixed part");
        return false;
    }

    // Allocate memory for base structure
    *cmd = (led_dynamic_command_t*)malloc(sizeof(led_dynamic_command_t));
    if (*cmd == NULL) {
        ESP_LOGE(TAG, "❌ malloc failed");
        return false;
    }

    // Parse fixed fields
    (*cmd)->strip_target = (led_strip_dynamic_target_t)data[(*offset)++];
    ESP_LOGI(TAG, "✅ strip_target=%d", (*cmd)->strip_target);
    
    memcpy(&(*cmd)->loop_duration_ms, &data[*offset], sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    ESP_LOGI(TAG, "✅ loop_duration_ms=%u", (unsigned)(*cmd)->loop_duration_ms);
    
    memcpy(&(*cmd)->keyframe_count, &data[*offset], sizeof(uint16_t));
    *offset += sizeof(uint16_t);
    ESP_LOGI(TAG, "✅ keyframe_count=%u", (*cmd)->keyframe_count);
    
    (*cmd)->loop_behavior = (loop_behavior_t)data[(*offset)++];
    ESP_LOGI(TAG, "✅ loop_behavior=%d", (*cmd)->loop_behavior);

    // Validate keyframe count
    if ((*cmd)->keyframe_count == 0 || (*cmd)->keyframe_count > MAX_KEYFRAMES) {
        ESP_LOGE(TAG, "❌ Invalid keyframe_count=%u (MAX=%d)", (*cmd)->keyframe_count, MAX_KEYFRAMES);
        free(*cmd);
        *cmd = NULL;
        return false;
    }

    // Reallocate with exact size for keyframes
    size_t total_size = sizeof(led_dynamic_command_t) + (*cmd)->keyframe_count * sizeof(led_keyframe_t);
    ESP_LOGI(TAG, "🔍 Realloc to %zu bytes for %u keyframes", total_size, (*cmd)->keyframe_count);
    
    led_dynamic_command_t* temp = (led_dynamic_command_t*)realloc(*cmd, total_size);
    if (temp == NULL) {
        ESP_LOGE(TAG, "❌ realloc failed");
        free(*cmd);
        *cmd = NULL;
        return false;
    }
    *cmd = temp;
    ESP_LOGI(TAG, "✅ Realloc OK");

    // Parse each keyframe
    for (uint16_t i = 0; i < (*cmd)->keyframe_count; i++) {
        ESP_LOGI(TAG, "🔍 Parsing keyframe %u/%u at offset=%zu", i+1, (*cmd)->keyframe_count, *offset);
        if (!parse_led_keyframe(data, offset, &(*cmd)->keyframes[i], (*cmd)->strip_target, data_len)) {
            ESP_LOGE(TAG, "❌ Failed to parse keyframe %u", i);
            free(*cmd);
            *cmd = NULL;
            return false;
        }
        ESP_LOGI(TAG, "✅ Keyframe %u OK", i);
    }
    
    ESP_LOGI(TAG, "✅ parse_led_dynamic SUCCESS: final offset=%zu", *offset);
    return true;
}

static bool parse_led_keyframe(const uint8_t* data, size_t* offset, led_keyframe_t* keyframe, led_strip_dynamic_target_t target, size_t data_len) {
    // Parse timestamp and transition
    if (*offset + sizeof(uint32_t) + sizeof(uint8_t) > data_len) return false;
    
    memcpy(&keyframe->timestamp_ms, &data[*offset], sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    
    keyframe->transition = (transition_mode_t)data[(*offset)++];

    // Parse colors based on target
    switch (target) {
        case ROOF_LED1_DYNAMIC: {
            size_t roof1_size = sizeof(led_roof1_strip_colors_t);
            if (*offset + roof1_size > data_len) return false;
            memcpy(&keyframe->colors.roof1, &data[*offset], roof1_size);
            *offset += roof1_size;
            break;
        }

        case ROOF_LED2_DYNAMIC: {
            size_t roof2_size = sizeof(led_roof2_strip_colors_t);
            if (*offset + roof2_size > data_len) return false;
            memcpy(&keyframe->colors.roof2, &data[*offset], roof2_size);
            *offset += roof2_size;
            break;
        }

        case ROOF_LED_ALL_DYNAMIC: {
            size_t roof1_size = sizeof(led_roof1_strip_colors_t);
            size_t roof2_size = sizeof(led_roof2_strip_colors_t);
            if (*offset + roof1_size + roof2_size > data_len) return false;
            
            memcpy(&keyframe->colors.both.roof1, &data[*offset], roof1_size);
            *offset += roof1_size;
            memcpy(&keyframe->colors.both.roof2, &data[*offset], roof2_size);
            *offset += roof2_size;
            break;
        }

        default:
            return false;
    }

    return true;
}

bool validate_parsed_command(const van_command_t* cmd) {
    if (cmd == NULL) return false;

    switch (cmd->type) {
        case COMMAND_TYPE_LED:
            return validate_led_command(&cmd->command.led_cmd);
        
        case COMMAND_TYPE_HEATER:
            // Validate temperature ranges
            return (cmd->command.heater_cmd.water_target_temp >= 0.0f && 
                    cmd->command.heater_cmd.water_target_temp <= 100.0f &&
                    cmd->command.heater_cmd.air_target_temp >= 0.0f &&
                    cmd->command.heater_cmd.air_target_temp <= 50.0f);
        
        case COMMAND_TYPE_HOOD:
            return (cmd->command.hood_cmd >= CMD_SET_TARGET_HOOD_OFF && 
                    cmd->command.hood_cmd <= CMD_SET_TARGET_HOOD_ON);
        
        case COMMAND_TYPE_WATER_CASE:
            return (cmd->command.water_case_cmd.cmd_case_number >= CASE_RST && 
                    cmd->command.water_case_cmd.cmd_case_number < CASE_MAX);
        
        case COMMAND_TYPE_MULTIMEDIA:
            return (cmd->command.videoprojecteur_cmd.cmd >= PROJECTOR_CMD_DEPLOY && 
                cmd->command.videoprojecteur_cmd.cmd <= PROJECTOR_CMD_CALIBRATE_DOWN);
        
        default:
            return false;
    }
}

static bool validate_led_command(const led_command_t* cmd) {
    if (cmd == NULL) return false;

    if (cmd->led_type == LED_STATIC) {
        const led_static_command_t* static_cmd = &cmd->command.static_cmd;
        return (static_cmd->strip_target >= ROOF_LED1 && 
                static_cmd->strip_target <= EXT_LED_ALL);
    } else if (cmd->led_type == LED_DYNAMIC) {
        // ✅ Accéder via le pointeur
        const led_dynamic_command_t* dynamic_cmd = cmd->command.dynamic_cmd;
        if (dynamic_cmd == NULL) return false;
        
        if (dynamic_cmd->keyframe_count == 0 || dynamic_cmd->keyframe_count > MAX_KEYFRAMES) {
            return false;
        }
        if (dynamic_cmd->loop_duration_ms == 0) {
            return false;
        }
        // Validate keyframe timestamps are in order
        for (uint16_t i = 1; i < dynamic_cmd->keyframe_count; i++) {
            if (dynamic_cmd->keyframes[i].timestamp_ms <= dynamic_cmd->keyframes[i-1].timestamp_ms) {
                return false;
            }
        }
        return true;
    }

    return false;
}

void free_van_command(van_command_t* cmd) {
    if (cmd) {
        // Libérer la commande dynamique si elle existe
        if (cmd->type == COMMAND_TYPE_LED && 
            cmd->command.led_cmd.led_type == LED_DYNAMIC &&
            cmd->command.led_cmd.command.dynamic_cmd != NULL) {
            free(cmd->command.led_cmd.command.dynamic_cmd);
        }
        free(cmd);
    }
}

const char* parse_result_to_string(command_parse_result_t result){
    switch (result) {
        case PARSE_SUCCESS:
            return "Parse Success";
        case PARSE_ERROR_INVALID_INPUT:
            return "Parse Error: Invalid Input";
        case PARSE_ERROR_INCOMPLETE_DATA:
            return "Parse Error: Incomplete Data";
        case PARSE_ERROR_MEMORY:
            return "Parse Error: Memory Allocation Failed";
        case PARSE_ERROR_UNKNOWN_TYPE:
            return "Parse Error: Unknown Command Type";
        case PARSE_ERROR_LED_DATA:
            return "Parse Error: LED Data Invalid";
        case PARSE_ERROR_VALIDATION_FAILED:
            return "Parse Error: Command Validation Failed";
        default:
            return "Parse Error: Unknown Error";
    }
}

// ============================================================================
// DEBUG FUNCTION (OPTIONAL)
#include <stdio.h>

const char* command_type_to_string(command_type_t type) {
    switch (type) {
        case COMMAND_TYPE_LED: return "LED";
        case COMMAND_TYPE_HEATER: return "HEATER";
        case COMMAND_TYPE_HOOD: return "HOOD";
        case COMMAND_TYPE_WATER_CASE: return "WATER_CASE";
        case COMMAND_TYPE_MULTIMEDIA: return "MULTIMEDIA";
        default: return "UNKNOWN";
    }
}

const char* led_type_to_string(led_type_t type) {
    switch (type) {
        case LED_STATIC: return "STATIC";
        case LED_DYNAMIC: return "DYNAMIC";
        default: return "UNKNOWN";
    }
}

const char* strip_target_to_string(led_strip_static_target_t target) {
    switch (target) {
        case ROOF_LED1: return "ROOF_LED1";
        case ROOF_LED2: return "ROOF_LED2";
        case ROOF_LED_ALL: return "ROOF_LED_ALL";
        case EXT_AV_LED: return "EXT_AV_LED";
        case EXT_AR_LED: return "EXT_AR_LED";
        case EXT_LED_ALL: return "EXT_LED_ALL";
        default: return "UNKNOWN";
    }
}

const char* loop_behavior_to_string(loop_behavior_t behavior) {
    switch (behavior) {
        case LOOP_BEHAVIOR_ONCE: return "ONCE";
        case LOOP_BEHAVIOR_REPEAT: return "REPEAT";
        case LOOP_BEHAVIOR_PING_PONG: return "PING_PONG";
        default: return "UNKNOWN";
    }
}

const char* transition_mode_to_string(transition_mode_t mode) {
    switch (mode) {
        case TRANSITION_LINEAR: return "LINEAR";
        case TRANSITION_EASE_IN_OUT: return "EASE_IN_OUT";
        case TRANSITION_STEP: return "STEP";
        default: return "UNKNOWN";
    }
}

const char* projector_command_to_string(projector_command_t cmd) {
    switch (cmd) {
        case PROJECTOR_CMD_DEPLOY: return "DEPLOY";
        case PROJECTOR_CMD_RETRACT: return "RETRACT";
        case PROJECTOR_CMD_STOP: return "STOP";
        case PROJECTOR_CMD_GET_STATUS: return "GET_STATUS";
        case PROJECTOR_CMD_JOG_UP_1: return "JOG_UP_1";
        case PROJECTOR_CMD_JOG_UP_01: return "JOG_UP_0.1";
        case PROJECTOR_CMD_JOG_UP_001: return "JOG_UP_0.01";
        case PROJECTOR_CMD_JOG_DOWN_1: return "JOG_DOWN_1";
        case PROJECTOR_CMD_JOG_DOWN_01: return "JOG_DOWN_0.1";
        case PROJECTOR_CMD_JOG_DOWN_001: return "JOG_DOWN_0.01";
        case PROJECTOR_CMD_JOG_UP_1_FORCED: return "JOG_UP_1_FORCED";
        case PROJECTOR_CMD_JOG_DOWN_1_FORCED: return "JOG_DOWN_1_FORCED";
        case PROJECTOR_CMD_CALIBRATE_UP: return "CALIBRATE_UP";
        case PROJECTOR_CMD_CALIBRATE_DOWN: return "CALIBRATE_DOWN";
        default: return "UNKNOWN";
    }
}

void print_led_color(const char* prefix, led_data_t color) {
    ESP_LOGI("CMD_DETAIL", "%s RGBW(%d,%d,%d,%d) Brightness:%d", 
             prefix, color.r, color.g, color.b, color.w, color.brightness);
}

void print_command_details(const van_command_t* cmd) {
    if (cmd == NULL) {
        ESP_LOGI("CMD_DETAIL", "Command is NULL");
        return;
    }
    
    ESP_LOGI("CMD_DETAIL", "=== COMMAND DETAILS ===");
    ESP_LOGI("CMD_DETAIL", "Type: %s (%d)", command_type_to_string(cmd->type), cmd->type);
    ESP_LOGI("CMD_DETAIL", "Timestamp: %u", (unsigned int)cmd->timestamp);
    
    switch (cmd->type) {
        case COMMAND_TYPE_LED: {
            const led_command_t* led_cmd = &cmd->command.led_cmd;
            ESP_LOGI("CMD_DETAIL", "LED Type: %s", led_type_to_string(led_cmd->led_type));
            
            if (led_cmd->led_type == LED_STATIC) {
                 const led_static_command_t* static_cmd = &led_cmd->command.static_cmd;
                ESP_LOGI("CMD_DETAIL", "Static Target: %s", strip_target_to_string(static_cmd->strip_target));
                
                // Afficher quelques couleurs d'exemple
                if (static_cmd->strip_target == ROOF_LED1 || static_cmd->strip_target == ROOF_LED_ALL) {
                    for (int i = 0; i < LED_STRIP_1_COUNT; i++) {
                        char prefix[20];
                        snprintf(prefix, sizeof(prefix), "Roof1 LED%d", i);
                        print_led_color(prefix, static_cmd->colors.roof.roof1_colors.color[i]);
                    }
                }
                if (static_cmd->strip_target == ROOF_LED2 || static_cmd->strip_target == ROOF_LED_ALL) {
                    for (int i = 0; i < LED_STRIP_2_COUNT; i++) {
                        char prefix[20];
                        snprintf(prefix, sizeof(prefix), "Roof2 LED%d", i);
                        print_led_color(prefix, static_cmd->colors.roof.roof2_colors.color[i]);
                    }
                }
            } else if (led_cmd->led_type == LED_DYNAMIC) {
                // ✅ Accéder via le pointeur
                const led_dynamic_command_t* dynamic_cmd = led_cmd->command.dynamic_cmd;
                if (dynamic_cmd != NULL) {
                    ESP_LOGI("CMD_DETAIL", "Dynamic Target: %d", dynamic_cmd->strip_target);
                    ESP_LOGI("CMD_DETAIL", "Loop Duration: %d ms", (int)dynamic_cmd->loop_duration_ms);
                    ESP_LOGI("CMD_DETAIL", "Keyframe Count: %d", dynamic_cmd->keyframe_count);
                    ESP_LOGI("CMD_DETAIL", "Loop Behavior: %s", loop_behavior_to_string(dynamic_cmd->loop_behavior));
                    
                    // Afficher les premiers keyframes
                    for (int i = 0; i < (dynamic_cmd->keyframe_count < 3 ? dynamic_cmd->keyframe_count : 3); i++) {
                        ESP_LOGI("CMD_DETAIL", "Keyframe %d: Time=%dms, Transition=%s", 
                                i, (int)dynamic_cmd->keyframes[i].timestamp_ms,
                                transition_mode_to_string(dynamic_cmd->keyframes[i].transition));
                        
                        // Afficher les couleurs du premier keyframe
                        if (i == 2) {
                            if (dynamic_cmd->strip_target == ROOF_LED1_DYNAMIC) {
                                for(int j = 0; j < LED_STRIP_1_COUNT; j++) {
                                    char prefix[30];
                                    snprintf(prefix, sizeof(prefix), "  KF0 Roof1 LED%d", j);
                                    print_led_color(prefix, dynamic_cmd->keyframes[i].colors.roof1.color[j]);
                                }
                            }
                            else if (dynamic_cmd->strip_target == ROOF_LED2_DYNAMIC) {
                                for(int j = 0; j < LED_STRIP_2_COUNT; j++) {
                                    char prefix[30];
                                    snprintf(prefix, sizeof(prefix), "  KF0 Roof2 LED%d", j);
                                    print_led_color(prefix, dynamic_cmd->keyframes[i].colors.roof2.color[j]);
                                }
                            } 
                            
                            else if (dynamic_cmd->strip_target == ROOF_LED_ALL_DYNAMIC) {
                                for (int j = 0; j < LED_STRIP_1_COUNT; j++) {
                                    char prefix[30];
                                    snprintf(prefix, sizeof(prefix), "  KF0 Roof1 LED%d", j);
                                    print_led_color(prefix, dynamic_cmd->keyframes[i].colors.both.roof1.color[j]);
                                }
                                for (int j = 0; j < LED_STRIP_2_COUNT; j++) {
                                    char prefix[30];
                                    snprintf(prefix, sizeof(prefix), "  KF0 Roof2 LED%d", j);
                                    print_led_color(prefix, dynamic_cmd->keyframes[i].colors.both.roof2.color[j]);
                                }
                            }
                        }
                    }
                } else {
                    ESP_LOGE("CMD_DETAIL", "Dynamic command is NULL!");
                }
            }
            break;
        }
        
        case COMMAND_TYPE_HEATER: {
            const heater_command_t* heater_cmd = &cmd->command.heater_cmd;
            ESP_LOGI("CMD_DETAIL", "Heater: %s", heater_cmd->heater_enabled ? "ON" : "OFF");
            ESP_LOGI("CMD_DETAIL", "Radiator Pump: %s", heater_cmd->radiator_pump_enabled ? "ON" : "OFF");
            ESP_LOGI("CMD_DETAIL", "Water Target: %.1f°C", heater_cmd->water_target_temp);
            ESP_LOGI("CMD_DETAIL", "Air Target: %.1f°C", heater_cmd->air_target_temp);
            ESP_LOGI("CMD_DETAIL", "Fan Speed: %d/255", heater_cmd->radiator_fan_speed);
            break;
        }
        
        case COMMAND_TYPE_HOOD: {
            ESP_LOGI("CMD_DETAIL", "Hood Command: %s", 
                    cmd->command.hood_cmd == CMD_SET_TARGET_HOOD_ON ? "ON" : "OFF");
            break;
        }
        
        case COMMAND_TYPE_WATER_CASE: {
            ESP_LOGI("CMD_DETAIL", "Water Case: %d", cmd->command.water_case_cmd.cmd_case_number);
            break;
        }
        
        case COMMAND_TYPE_MULTIMEDIA: {
            ESP_LOGI("CMD_DETAIL", "Multimedia Command: %s (0x%02X)", 
                    projector_command_to_string(cmd->command.videoprojecteur_cmd.cmd),
                    cmd->command.videoprojecteur_cmd.cmd);
            break;
        }
        
        default:
            ESP_LOGI("CMD_DETAIL", "Unknown command type");
            break;
    }
    ESP_LOGI("CMD_DETAIL", "=== END COMMAND DETAILS ===");
}