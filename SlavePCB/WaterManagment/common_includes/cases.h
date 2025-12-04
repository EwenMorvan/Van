#ifndef CASES_H
#define CASES_H

// System cases
typedef enum {
    CASE_RST = 0,  // Reset
    CASE_E1,       // Evier EP -> ES
    CASE_E2,       // Evier EP -> ER
    CASE_E3,       // Evier ER -> ES
    CASE_E4,       // Evier ER -> ER
    CASE_D1,       // Douche EP -> ES
    CASE_D2,       // Douche EP -> ER
    CASE_D3,       // Douche ER -> ES
    CASE_D4,       // Douche ER -> ER
    CASE_V1,       // Vidange ES -> V
    CASE_V2,       // Vidange ER -> V
    CASE_P1,       // Recup Pluie -> ER
    CASE_MAX
} system_case_t;

#endif // CASES_H