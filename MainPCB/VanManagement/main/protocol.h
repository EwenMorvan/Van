#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "log_level.h"

#define MAX_LED_MODES 5
#define MAX_LED_COUNT 120

// System state structure
typedef struct {
    // MPPT data
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
    } mppt;
    
    // Sensor data
    struct {
        float fuel_level;
        float onboard_temperature;
        float cabin_temperature;
        float humidity;
        uint16_t co2_level;
        uint16_t light_level;
        bool van_light_active;
        bool door_open;
    } sensors;
    
    // Heater data
    struct {
        bool heater_on;
        float water_temperature;
        float target_water_temp;
        float target_cabin_temp;
        bool pump_active;
        uint8_t radiator_fan_speed;
    } heater;
    
    // Fan states
    struct {
        uint8_t elec_box_speed;
        uint8_t heater_fan_speed;
        bool hood_fan_active;
    } fans;
    
    // LED states
    struct {
        struct {
            uint8_t current_mode;
            uint8_t brightness;
            bool enabled;
            bool switch_pressed;
            uint32_t last_switch_time;
        } roof;
        
        struct {
            uint8_t current_mode;
            uint8_t brightness;
            bool power_enabled;
        } exterior;
        
        bool error_mode_active;
    } leds;
    
    // System status
    struct {
        bool system_error;
        uint32_t error_code;
        uint32_t uptime;
        bool slave_pcb_connected;
    } system;
} van_state_t;

// LED mode configuration
typedef struct {
    uint8_t red[MAX_LED_COUNT];
    uint8_t green[MAX_LED_COUNT];
    uint8_t blue[MAX_LED_COUNT];
    uint8_t white[MAX_LED_COUNT];
    uint8_t brightness;
    uint16_t animation_speed;
    bool enabled;
} led_mode_t;

// Command types
typedef enum {
    CMD_SET_HEATER_TARGET,
    CMD_SET_HEATER_STATE,
    CMD_SET_LED_STATE,
    CMD_SET_LED_MODE,
    CMD_SET_LED_BRIGHTNESS,
    CMD_SET_FAN_SPEED,
    CMD_SET_HOOD_STATE,
    CMD_REQUEST_STATUS,
    CMD_SAVE_LED_MODE,
    CMD_SET_LED_REALTIME
} command_type_t;

// Communication command structure
typedef struct {
    command_type_t type;
    uint8_t target;  // Which device/component
    uint32_t value;  // Command value
    uint8_t data[256]; // Additional data
} van_command_t;

// Error codes (bit flags)
#define ERROR_NONE              0x00
#define ERROR_HEATER_NO_FUEL    0x01    // Bit 0
#define ERROR_MPPT_COMM         0x02    // Bit 1
#define ERROR_SENSOR_COMM       0x04    // Bit 2
#define ERROR_SLAVE_COMM        0x08    // Bit 3
#define ERROR_LED_STRIP         0x10    // Bit 4
#define ERROR_FAN_CONTROL       0x20    // Bit 5

// Function prototypes
void protocol_init(void);
void protocol_update_state(van_state_t *state);
van_state_t* protocol_get_state(void);
void protocol_process_command(van_command_t *cmd);
void protocol_set_error(uint32_t error_code);
void protocol_clear_error_flag(uint32_t error_code);
void protocol_clear_error(void);

// SIMULATION FUNCTIONS - Remove when real hardware is connected
#define ENABLE_SIMULATION 0  // Set to 0 to disable simulation
#if ENABLE_SIMULATION
void protocol_simulate_sensor_data(void);
#endif

#endif // PROTOCOL_H
