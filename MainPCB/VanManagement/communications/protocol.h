#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "../common_includes/error_manager.h"
#include "../common_includes/slave_pcb_res/slave_pcb_state.h"
#include "../common_includes/slave_pcb_res/slave_pcb_cases.h"
#include "../common_includes/gpio_pinout.h"


// ============================================================================
// COMMAND TYPES
// ============================================================================

// ============================== LED GENERIC COMMAND STRUCTURES ==============================

typedef enum {
    LED_STATIC,
    LED_DYNAMIC,
} led_type_t;

// Cibles pour STATIC (Roof + Ext)
typedef enum {
    // Roof targets
    ROOF_LED1,
    ROOF_LED2, 
    ROOF_LED_ALL,
    // Ext targets  
    EXT_AV_LED,
    EXT_AR_LED,
    EXT_LED_ALL,
} led_strip_static_target_t;

// Cibles pour DYNAMIC (Roof seulement)
typedef enum {
    ROOF_LED1_DYNAMIC,
    ROOF_LED2_DYNAMIC,
    ROOF_LED_ALL_DYNAMIC,
} led_strip_dynamic_target_t;

typedef struct {
    uint8_t r;
    uint8_t g; 
    uint8_t b;
    uint8_t w;
    uint8_t brightness;
} led_data_t;

// ============================== LED STATIC COMMAND STRUCTURES ==============================

typedef struct {
    led_data_t color[LED_STRIP_1_COUNT];
} led_roof1_strip_colors_t;

typedef struct {
    led_data_t color[LED_STRIP_2_COUNT];
} led_roof2_strip_colors_t;

typedef struct {
    led_data_t color[LED_STRIP_EXT_FRONT_COUNT];
} led_ext_av_strip_colors_t;

typedef struct {
    led_data_t color[LED_STRIP_EXT_BACK_COUNT];
} led_ext_ar_strip_colors_t;

// Static: peut cibler Roof OU Ext (mais pas les deux en même temps)
typedef struct {
    led_strip_static_target_t strip_target;
    union {
        // Roof data
        struct {
            led_roof1_strip_colors_t roof1_colors;
            led_roof2_strip_colors_t roof2_colors;
        } roof;
        // Ext data  
        struct {
            led_ext_av_strip_colors_t ext_av_colors;
            led_ext_ar_strip_colors_t ext_ar_colors;
        } ext;
    } colors;
} led_static_command_t;

// =============================== LED DYNAMIC COMMAND STRUCTURES ==============================

typedef enum {
    LOOP_BEHAVIOR_ONCE,
    LOOP_BEHAVIOR_REPEAT, 
    LOOP_BEHAVIOR_PING_PONG,
} loop_behavior_t;

typedef enum {
    TRANSITION_LINEAR,
    TRANSITION_EASE_IN_OUT,
    TRANSITION_STEP,
} transition_mode_t;

// Dynamic: UNIQUEMENT Roof, mais peut cibler 1, 2 ou 1+2
typedef struct {
    uint32_t timestamp_ms;
    transition_mode_t transition;
    union {
        led_roof1_strip_colors_t roof1;
        led_roof2_strip_colors_t roof2;
        struct {
            led_roof1_strip_colors_t roof1;
            led_roof2_strip_colors_t roof2;
        } both;
    } colors;
} led_keyframe_t;

typedef struct {
    led_strip_dynamic_target_t strip_target; // Seulement roof!
    uint32_t loop_duration_ms;
    uint16_t keyframe_count;
    loop_behavior_t loop_behavior;
    led_keyframe_t keyframes[];
} led_dynamic_command_t;

// ============================== LED FINAL COMMAND STRUCTURES ==============================

typedef struct {
    led_type_t led_type;
    union {
        led_static_command_t static_cmd;
        led_dynamic_command_t* dynamic_cmd;
    } command;
} led_command_t;


// ============================== GENERIC COMMAND STRUCTURES ==============================
typedef struct {
    // États
    bool heater_enabled;
    bool radiator_pump_enabled;
    // Consignes
    float water_target_temp;
    float air_target_temp; 
    uint8_t radiator_fan_speed; // 0-255
} heater_command_t;

typedef enum {
    CMD_SET_TARGET_HOOD_OFF,
    CMD_SET_TARGET_HOOD_ON,
} hood_command_t;

typedef struct {
    system_case_t cmd_case_number;
} water_case_command_t;

// ============================== VIDEO PROJECTOR COMMAND STRUCTURES ==============================
typedef enum {
    PROJECTOR_CMD_DEPLOY = 0,        // Deploie completement (0x00)
    PROJECTOR_CMD_RETRACT = 1,       // Retracte completement (0x01)
    PROJECTOR_CMD_STOP = 2,          // Arrete le moteur (0x02)
    PROJECTOR_CMD_GET_STATUS = 3,    // Demande le statut (0x03)
    PROJECTOR_CMD_JOG_UP_1 = 4,      // Avance de 1.0 tour (0x04)
    PROJECTOR_CMD_JOG_UP_01 = 5,     // Avance de 0.1 tour (0x05)
    PROJECTOR_CMD_JOG_UP_001 = 6,    // Avance de 0.01 tour (0x06)
    PROJECTOR_CMD_JOG_DOWN_1 = 7,    // Recule de 1.0 tour (0x07)
    PROJECTOR_CMD_JOG_DOWN_01 = 8,   // Recule de 0.1 tour (0x08)
    PROJECTOR_CMD_JOG_DOWN_001 = 9,  // Recule de 0.01 tour (0x09)
} projector_command_t;

typedef struct {
    projector_command_t cmd;
} videoprojecteur_command_t;

typedef enum {
    PROJECTOR_STATE_UNKNOWN = 0,
    PROJECTOR_STATE_RETRACTED = 1,
    PROJECTOR_STATE_RETRACTING = 2,
    PROJECTOR_STATE_DEPLOYED = 3,
    PROJECTOR_STATE_DEPLOYING = 4,
} projector_state_t;

typedef enum {
    COMMAND_TYPE_LED,
    COMMAND_TYPE_HEATER,
    COMMAND_TYPE_HOOD,
    COMMAND_TYPE_WATER_CASE,
    COMMAND_TYPE_MULTIMEDIA,
} command_type_t;

// Communication command structure
typedef struct {
    command_type_t type;
    uint32_t timestamp;
    union {
        led_command_t led_cmd;
        heater_command_t heater_cmd;
        hood_command_t hood_cmd;
        water_case_command_t water_case_cmd;
        videoprojecteur_command_t videoprojecteur_cmd;
    } command;
   
} van_command_t;

// ============================================================================
// STATE STRUCTURES
// ============================================================================
typedef enum {
    CHARGE_STATE_OFF = 0,
    CHARGE_STATE_BULK,
    CHARGE_STATE_ABSORPTION,
    CHARGE_STATE_FLOAT,
    CHARGE_STATE_EQUALIZE,
    CHARGE_STATE_STORAGE,
} charge_state_t;

// Main PCB and devices state
typedef struct {
    // ═══════════════════════════════════════════════════════════
    // ÉNERGIE - MPPT Solar Charge Controllers
    // ═══════════════════════════════════════════════════════════
    struct {
        // MPPT 100|50 (Panneaux principal: 4x130W en 2s2p, 48V nominal)
        float solar_power_100_50;          // Puissance panneau (W)
        float panel_voltage_100_50;        // Tension panneau (V)
        float panel_current_100_50;        // Courant panneau (A)
        float battery_voltage_100_50;      // Tension batterie (V)
        float battery_current_100_50;      // Courant batterie (A)
        int8_t temperature_100_50;         // Température (°C)
        charge_state_t state_100_50;              // État chargeur (0-6)
        uint16_t error_flags_100_50;       // Flags d'erreur
        
        // MPPT 70|15 (Panneaux secondaire: 2x100W en série, 48V nominal)
        float solar_power_70_15;
        float panel_voltage_70_15;         // Tension panneau (V)
        float panel_current_70_15;         // Courant panneau (A)
        float battery_voltage_70_15;
        float battery_current_70_15;
        int8_t temperature_70_15;
        charge_state_t state_70_15;
        uint16_t error_flags_70_15;
    } mppt;
    // ═══════════════════════════════════════════════════════════
    // ÉNERGIE - Alternanceur/chargeur
    // ═══════════════════════════════════════════════════════════
     struct {
        charge_state_t state;
        float input_voltage;        // Tension secteur (V)
        float output_voltage;       // Tension sortie onduleur (V)
        float output_current;       // Courant sortie onduleur (A)
     } alternator_charger;
    // ═══════════════════════════════════════════════════════════
    // ÉNERGIE - Inverter/charger
    // ═══════════════════════════════════════════════════════════
    struct {
        bool enabled;
        float ac_input_voltage;        // Tension secteur (V)
        float ac_input_frequency;      // Fréquence secteur (Hz)
        float ac_input_current;        // Courant secteur (A)
        float ac_input_power;          // Puissance secteur (W)
        float ac_output_voltage;       // Tension sortie onduleur (V)
        float ac_output_frequency;     // Fréquence sortie onduleur (Hz)
        float ac_output_current;       // Courant sortie onduleur (A)
        float ac_output_power;         // Puissance sortie onduleur (W)
        float battery_voltage;         // Tension batterie (V)
        float battery_current;         // Courant batterie (A)
        float inverter_temperature;    // Température onduleur (°C)
        charge_state_t charger_state;         // État chargeur (0=Off, 1=Bulk, 2=Absorption, 3=Float, etc.)
        uint16_t error_flags;          // Flags d'erreur
    } inverter_charger;
    // ═══════════════════════════════════════════════════════════
    // ÉNERGIE - Battery State (from BLE battery service)
    // ═══════════════════════════════════════════════════════════
    struct {
        // Basic measurements
        uint16_t voltage_mv;        // Total battery voltage in millivolts (e.g., 13200 = 13.2V)
        int16_t current_ma;         // Battery current in milliamps (positive = charging, negative = discharging)
        uint32_t capacity_mah;    // Capacité restante (mAh)
        uint8_t soc_percent;        // State of Charge in percent (0-100%)

        // Cell information
        uint8_t cell_count;         // Number of cells
        uint16_t cell_voltage_mv[16];  // Individual cell voltages in mV  (max 16 cells)

        // Temperature sensors
        uint8_t temp_sensor_count;  // Number of temperature sensors
        int16_t temperatures_c[8];  // Temperature values in Celsius (max 8 sensors)

        // Additional info
        uint16_t cycle_count;       // Number of full charge/discharge cycles
        uint32_t nominal_capacity_mah;  // Current full capacity measured by BMS (can degrade over time)
        uint32_t design_capacity_mah;   // Factory design capacity (e.g., 300,000 mAh = 300 Ah)
        uint8_t health_percent;     // Battery health (nominal / design × 100%)

        /* BMS status fields (JBD Smart BMS V4):
         * - mosfet_status: 8-bit bitfield indicating MOSFET outputs.
         *     Bit 0 = charge MOSFET (1 = ON), Bit 1 = discharge MOSFET (1 = ON).
         *     Bits 2..7 are reserved.
         * - protection_status: 16-bit bitfield of protection flags (1 = active).
         *     bit0: single cell overvoltage
         *     bit1: single cell undervoltage
         *     bit2: whole pack overvoltage
         *     bit3: whole pack undervoltage
         *     bit4: charging over-temperature
         *     bit5: charging low temperature
         *     bit6: discharge over temperature
         *     bit7: discharge low temperature
         *     bit8: charging overcurrent
         *     bit9: discharge overcurrent
         *     bit10: short circuit protection
         *     bit11: front-end detection IC error
         *     bit12: software lock MOS
         *     bit13..bit15: reserved
         * - balance_status: 32-bit bitfield describing active cell balancing.
         *     Lower 16 bits = cells 1..16 (low word), upper 16 bits = cells 17..32 (high word).
         *     A bit value of 1 means balancing is active for that cell.
         *
         * Note: JBD protocol transmits 16-bit words in big-endian (high byte first).
         * To reconstruct `balance_status` from two 16-bit words (low, high):
         *   balance_status = ((uint32_t)high << 16) | (uint32_t)low;
         */

        uint8_t mosfet_status;      // Bit 0=charge MOSFET, Bit 1=discharge MOSFET
        uint16_t protection_status; // Protection flags (overvoltage, undervoltage, etc.)
        uint32_t balance_status;    // Cell balance status bitfield (low=1..16, high=17..32)
    } battery;
    
    // ═══════════════════════════════════════════════════════════
    // CAPTEURS - Monitoring environnemental
    // ═══════════════════════════════════════════════════════════
    struct {
        float cabin_temperature;       // Température intérieure (°C)
        float exterior_temperature;    // Température extérieure (°C)
        float humidity;                // Humidité relative (%)
        uint16_t co2_level;            // Niveau CO2 (ppm)
        bool door_open;                // Porte ouverte/fermée
    } sensors;
    
    // ═══════════════════════════════════════════════════════════
    // CHAUFFAGE - Diesel water heater + Air heater
    // ═══════════════════════════════════════════════════════════
    struct {
        bool heater_on;                // Chauffage diesel ON/OFF
        float target_air_temperature;  // Consigne température de l'air (°C)
        float actual_air_temperature;  // Température actuelle de l'air (°C)
        float antifreeze_temperature;  // Température antigel circuit (°C)
        uint8_t fuel_level_percent;    // Niveau carburant (%)
        uint16_t error_code;           // Code erreur chauffage
        bool pump_active;              // Pompe circulation active
        uint8_t radiator_fan_speed;    // Vitesse ventilateur (0-100%)
    } heater;
    
    // ═══════════════════════════════════════════════════════════
    // ÉCLAIRAGE - LED strips (Interior + Exterior)
    // ═══════════════════════════════════════════════════════════
    struct {
        // LEDs intérieures (plafond - 2 strips)
        struct {
            bool enabled;              // Activé/Désactivé
            uint8_t current_mode;      // Mode: 0=OFF, 1=WHITE, 2=ORANGE, etc.
            uint8_t brightness;        // Luminosité (0-255)
        } leds_roof1;

        struct {
            bool enabled;
            uint8_t current_mode;
            uint8_t brightness;
        } leds_roof2;
        
        // LEDs extérieures (avant)
        struct {
            bool enabled;
            uint8_t current_mode;
            uint8_t brightness;
        } leds_av;
        
        // LEDs extérieures (arrière)
        struct {
            bool enabled;
            uint8_t current_mode;
            uint8_t brightness;
        } leds_ar;
    } leds;
    
    // ═══════════════════════════════════════════════════════════
    // SYSTÈME - Status général & erreurs
    // ═══════════════════════════════════════════════════════════

    struct {
        uint32_t uptime;                // Temps de fonctionnement (s)
        bool system_error;              // Erreur système présente
        uint32_t error_code;            // Code erreur global
        main_error_state_t errors;      // État détaillé des erreurs Main PCB
    } system;
    
    // ═══════════════════════════════════════════════════════════
    // VIDEOPROJECTEUR - Motorise (commandes BLE)
    // ═══════════════════════════════════════════════════════════
    struct {
        projector_state_t state;        // Etat du videoprojecteur (Retracted/Deploying/Deployed/Retracting)
        bool connected;                 // Connecte via BLE
        uint32_t last_update_time;      // Timestamp de la derniere mise a jour
    } videoprojecteur;
    
    // ═══════════════════════════════════════════════════════════
    // SLAVE PCB - Etat de la carte esclave
    // ═══════════════════════════════════════════════════════════
    slave_pcb_state_t slave_pcb;        // Etat complet du Slave PCB
    
} van_state_t;




// ============================================================================
// PROTOCOL FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the protocol module
 * @return ESP_OK on success
 */
esp_err_t protocol_init(void);

/**
 * @brief Get pointer to the global van state
 * 
 * Returns a direct pointer to van_state_t for reading and updating.
 * Each manager should update only its own section.
 * 
 * @return Pointer to van_state_t structure (never NULL)
 */
van_state_t* protocol_get_van_state(void);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Update system uptime
 */
void protocol_update_uptime(void);

/**
 * @brief Get system uptime in seconds
 * @return Current system uptime
 */
uint32_t protocol_get_uptime(void);

/**
 * @brief Set system error state
 * @param error True if system has error
 * @param error_code Error code (0 if no error)
 */
void protocol_set_system_error(bool error, uint32_t error_code);

/**
 * @brief Get current system error state
 * @param[out] error_code Pointer to store error code (can be NULL)
 * @return True if system has error
 */
bool protocol_has_system_error(uint32_t* error_code);

/**
 * @brief Print current van state summary for debugging
 */
void protocol_print_state_summary(void);

#endif // PROTOCOL_H
