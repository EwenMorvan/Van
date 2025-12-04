#ifndef UTILS_H
#define UTILS_H

#include "error_manager.h"
#include "cases.h"
#include "buttons.h"


// Function declarations
const char* get_error_string(slave_pcb_err_t error);
const char* get_case_string(system_case_t case_id);
const char* get_button_string(button_type_t button);

#endif // UTILS_H