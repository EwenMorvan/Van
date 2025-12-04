#ifndef BUTTONS_H
#define BUTTONS_H
#include <stdint.h>
#include <stdbool.h>

#include "gpio_pinout.h"
#include "cases.h"

#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_CLICK_MS 1000
// Button types
typedef enum {
    BUTTON_BE1 = 0,
    BUTTON_BE2,
    BUTTON_BD1,
    BUTTON_BD2,
    BUTTON_BH,
    BUTTON_BV1,
    BUTTON_BV2,
    BUTTON_BP1,
    BUTTON_BRST,
    BUTTON_MAX
} button_type_t;

// Button click types
typedef enum {
    CLICK_NONE = 0,
    CLICK_SHORT,
    CLICK_LONG,
    CLICK_PERMANENT
} click_type_t;

// Button to GPIO mapping
static const int button_gpio_map[BUTTON_MAX] = {
    [BUTTON_BE1] = BE1,
    [BUTTON_BE2] = BE2,
    [BUTTON_BD1] = BD1,
    [BUTTON_BD2] = BD2,
    [BUTTON_BH] = BH,
    [BUTTON_BV1] = -1,  // Virtual button
    [BUTTON_BV2] = -1,  // Virtual button
    [BUTTON_BP1] = -1,  // Virtual button
    [BUTTON_BRST] = -1, // Virtual button
};

// Button state tracking
typedef struct {
    bool current_state;
    bool previous_state;
    uint32_t press_start_time;
    uint32_t last_change_time;
    click_type_t last_click;
    bool virtual_state; // For virtual buttons from MainPCB
} button_state_t;

#endif // BUTTONS_H