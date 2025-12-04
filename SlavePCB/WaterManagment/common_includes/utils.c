#include "utils.h"


const char* get_case_string(system_case_t case_id) {
    switch (case_id) {
        case CASE_RST: return "RST (All Electrovalves in default configuration and Pumps Off)";
        case CASE_E1: return "E1 (Clean Water Sink to Dirty Water)";
        case CASE_E2: return "E2 (Clean Water Sink to Recycled Water)";
        case CASE_E3: return "E3 (Recycled Water Sink to Dirty Water)";
        case CASE_E4: return "E4 (Recycled Water Sink to Recycled Water)";
        case CASE_D1: return "D1 (Shower Clean Water to Dirty Water)";
        case CASE_D2: return "D2 (Shower Clean Water to Recycled Water)";
        case CASE_D3: return "D3 (Shower Recycled Water to Dirty Water)";
        case CASE_D4: return "D4 (Shower Recycled Water to Recycled Water)";
        case CASE_V1: return "V1 (Dirty Water to Drain)";
        case CASE_V2: return "V2 (Recycled Water to Drain)";
        case CASE_P1: return "P1 (Rainwater Recovery to Recycled Water)";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Get button name string from button ID
 */
const char* get_button_string(button_type_t button) {
    switch (button) {
        case BUTTON_BE1: return "BE1";
        case BUTTON_BE2: return "BE2";
        case BUTTON_BD1: return "BD1";
        case BUTTON_BD2: return "BD2";
        case BUTTON_BH: return "BH";
        case BUTTON_BV1: return "BV1";
        case BUTTON_BV2: return "BV2";
        case BUTTON_BP1: return "BP1";
        case BUTTON_BRST: return "BRST";
        default: return "UNKNOWN_BUTTON";
    }
}