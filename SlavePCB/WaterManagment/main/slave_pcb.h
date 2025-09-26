#ifndef SLAVE_PCB_H
#define SLAVE_PCB_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

#include "sdkconfig.h"

#include "gpio_pinout.h"

// Error codes
typedef enum {
    SLAVE_PCB_OK = 0,
    SLAVE_PCB_ERR_INVALID_ARG,
    SLAVE_PCB_ERR_DEVICE_NOT_FOUND,
    SLAVE_PCB_ERR_STATE_INVALID,
    SLAVE_PCB_ERR_INCOMPATIBLE_CASE,
    SLAVE_PCB_ERR_TIMEOUT,
    SLAVE_PCB_ERR_I2C_FAIL,
    SLAVE_PCB_ERR_SPI_FAIL,
    SLAVE_PCB_ERR_MEMORY,
    SLAVE_PCB_ERR_COMM_FAIL
} slave_pcb_err_t;

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

// Device types
typedef enum {
    DEVICE_ELECTROVALVE_A = 0,
    DEVICE_ELECTROVALVE_B,
    DEVICE_ELECTROVALVE_C,
    DEVICE_ELECTROVALVE_D,
    DEVICE_ELECTROVALVE_E,
    DEVICE_ELECTROVALVE_F,
    DEVICE_PUMP_PE,
    DEVICE_PUMP_PD,
    DEVICE_PUMP_PV,
    DEVICE_PUMP_PP,
    DEVICE_LED_BE1_RED,
    DEVICE_LED_BE1_GREEN,
    DEVICE_LED_BE2_RED,
    DEVICE_LED_BE2_GREEN,
    DEVICE_LED_BD1_RED,
    DEVICE_LED_BD1_GREEN,
    DEVICE_LED_BD2_RED,
    DEVICE_LED_BD2_GREEN,
    DEVICE_LED_BH,
    DEVICE_MAX
} device_type_t;

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
    CLICK_LONG
} click_type_t;

// System states
typedef enum {
    STATE_CE = (1 << 0),  // Clean Water Empty
    STATE_DF = (1 << 1),  // Dirt Water Full
    STATE_DE = (1 << 2),  // Dirt Water Empty
    STATE_RF = (1 << 3),  // Recycled Water Full
    STATE_RE = (1 << 4)   // Recycled Water Empty
} system_states_t;

// HX711 tank IDs
typedef enum {
    TANK_A = 0,
    TANK_B,
    TANK_C,
    TANK_D,
    TANK_E,
    TANK_MAX
} tank_id_t;

// Communication message types
typedef enum {
    MSG_CASE_CHANGE = 0,
    MSG_BUTTON_STATE,
    MSG_LOAD_CELL_DATA,
    MSG_DEVICE_STATUS,
    MSG_ERROR,
    MSG_RST_REQUEST,
    MSG_MAX
} msg_type_t;

// Communication message structure
typedef struct {
    msg_type_t type;
    uint32_t timestamp;
    union {
        system_case_t case_data;
        struct {
            button_type_t button;
            bool state;
        } button_data;
        struct {
            tank_id_t tank;
            float weight;
        } load_cell_data;
        struct {
            device_type_t device;
            bool status;
        } device_status;
        struct {
            slave_pcb_err_t error_code;
            char description[64];
        } error_data;
    } data;
} comm_msg_t;

// Function declarations
slave_pcb_err_t set_output_state(device_type_t device, bool state);
bool is_case_compatible(system_case_t case_id, uint32_t system_states);
const char* get_error_string(slave_pcb_err_t error);
const char* get_case_string(system_case_t case_id);

// Manager initialization functions
slave_pcb_err_t electrovalve_pump_manager_init(void);
slave_pcb_err_t button_manager_init(void);
slave_pcb_err_t load_cell_manager_init(void);
slave_pcb_err_t communication_manager_init(void);

// Manager task functions
void electrovalve_pump_manager_task(void *pvParameters);
void button_manager_task(void *pvParameters);
void load_cell_manager_task(void *pvParameters);
void communication_manager_task(void *pvParameters);
void button_manager_notify_transition_complete(void);

// Additional function declarations
slave_pcb_err_t apply_electrovalve_pump_case(system_case_t case_id);
uint32_t get_current_system_states(void);

// Load cell calibration functions
slave_pcb_err_t calibrate_tank_with_known_weight(tank_id_t tank_id, float known_weight_kg);

// Current reading functions  
float get_pump_current_reading(uint8_t pump_id);
float get_electrovalve_current_reading(uint8_t electrovalve_id);

// Error management functions
void log_system_error(slave_pcb_err_t error_code, const char* component, const char* description);
void check_system_health(void);
void print_system_status(void);
void reset_error_counters(void);

// Shift register functions
slave_pcb_err_t init_shift_registers(void);
slave_pcb_err_t shift_register_set_output_state(device_type_t device, bool state);
bool get_device_state(device_type_t device);
slave_pcb_err_t set_all_outputs_safe(void);
slave_pcb_err_t enable_shift_register_outputs(bool enable);
void print_shift_register_state(void);

// Global queues for inter-task communication
extern QueueHandle_t comm_queue;
extern QueueHandle_t case_queue;
extern QueueHandle_t button_queue;
extern QueueHandle_t loadcell_queue;

// Global mutex for shared resources
extern SemaphoreHandle_t output_mutex;

#endif // SLAVE_PCB_H
