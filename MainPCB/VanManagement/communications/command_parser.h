// #ifndef COMMAND_PARSER_H
// #define COMMAND_PARSER_H

// #include "esp_err.h"
// #include "protocol.h"

// /**
//  * @brief Parse raw BLE command data into van_command_t structure
//  * 
//  * @param data Raw data received from BLE
//  * @param len Length of data (must be sizeof(van_command_t))
//  * @param out_cmd Pointer to van_command_t structure to fill
//  * @return ESP_OK on success, ESP_ERR_INVALID_ARG if invalid size
//  */
// esp_err_t command_parse(const uint8_t* data, size_t len, van_command_t* out_cmd);

// #endif // COMMAND_PARSER_H

#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "protocol.h"  // Tes structures de commandes


// Configuration
#define MIN_VAN_COMMAND_SIZE (sizeof(uint8_t) + sizeof(uint32_t))  // type + timestamp
#define MAX_KEYFRAMES 100

// Parse results
typedef enum {
    PARSE_SUCCESS = 0,
    PARSE_ERROR_INVALID_INPUT,
    PARSE_ERROR_INCOMPLETE_DATA,
    PARSE_ERROR_MEMORY,
    PARSE_ERROR_UNKNOWN_TYPE,
    PARSE_ERROR_LED_DATA,
    PARSE_ERROR_VALIDATION_FAILED,
} command_parse_result_t;

// Public API
command_parse_result_t parse_van_command(const uint8_t* raw_data, size_t data_len, van_command_t** output_cmd);
bool validate_parsed_command(const van_command_t* cmd);
void free_van_command(van_command_t* cmd);

// Helper function to get parse result as string
const char* parse_result_to_string(command_parse_result_t result);

void print_command_details(const van_command_t* cmd);

#endif // COMMAND_PARSER_H