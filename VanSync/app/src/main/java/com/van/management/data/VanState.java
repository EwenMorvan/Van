package com.van.management.data;


public class VanState {

    // ═══════════════════════════════════════════════════════════
    // ENUMS
    // ═══════════════════════════════════════════════════════════
    public enum ChargeState {
        CHARGE_STATE_OFF,
        CHARGE_STATE_BULK,
        CHARGE_STATE_ABSORPTION,
        CHARGE_STATE_FLOAT,
        CHARGE_STATE_EQUALIZE,
        CHARGE_STATE_STORAGE
    }

    public enum ProjectorState {
        PROJECTOR_STATE_UNKNOWN(0),
        PROJECTOR_STATE_RETRACTED(1),
        PROJECTOR_STATE_RETRACTING(2),
        PROJECTOR_STATE_DEPLOYED(3),
        PROJECTOR_STATE_DEPLOYING(4);
        
        private final int value;
        ProjectorState(int value) { this.value = value; }
        public int getValue() { return value; }
        
        public static ProjectorState fromValue(int value) {
            for (ProjectorState state : ProjectorState.values()) {
                if (state.value == value) return state;
            }
            return PROJECTOR_STATE_UNKNOWN;
        }
    }

    // ═══════════════════════════════════════════════════════════
    // ÉNERGIE - MPPT Solar Charge Controllers
    // ═══════════════════════════════════════════════════════════
    public static class MpptData {
        public float solar_power_100_50;
        public float panel_voltage_100_50;
        public float panel_current_100_50;
        public float battery_voltage_100_50;
        public float battery_current_100_50;
        public byte temperature_100_50;
        public ChargeState state_100_50;
        public int error_flags_100_50;

        public float solar_power_70_15;
        public float panel_voltage_70_15;
        public float panel_current_70_15;
        public float battery_voltage_70_15;
        public float battery_current_70_15;
        public byte temperature_70_15;
        public ChargeState state_70_15;
        public int error_flags_70_15;

        public MpptData() {}
    }

    // ═══════════════════════════════════════════════════════════
    // ÉNERGIE - Alternateur/chargeur
    // ═══════════════════════════════════════════════════════════
    public static class AlternatorChargerData {
        public ChargeState state;
        public float input_voltage;
        public float output_voltage;
        public float output_current;

        public AlternatorChargerData() {}
    }

    // ═══════════════════════════════════════════════════════════
    // ÉNERGIE - Inverter/charger
    // ═══════════════════════════════════════════════════════════
    public static class InverterChargerData {
        public boolean enabled;
        public float ac_input_voltage;
        public float ac_input_frequency;
        public float ac_input_current;
        public float ac_input_power;
        public float ac_output_voltage;
        public float ac_output_frequency;
        public float ac_output_current;
        public float ac_output_power;
        public float battery_voltage;
        public float battery_current;
        public float inverter_temperature;
        public ChargeState charger_state;
        public int error_flags;

        public InverterChargerData() {}
    }

    // ═══════════════════════════════════════════════════════════
    // ÉNERGIE - Battery State (from BLE battery service)
    // ═══════════════════════════════════════════════════════════
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

    public static class BatteryData {
        public int voltage_mv;
        public int current_ma;
        public long capacity_mah;
        public byte soc_percent;

        public byte cell_count;
        public int[] cell_voltage_mv = new int[16];

        public byte temp_sensor_count;
        public int[] temperatures_c = new int[8];

        public int cycle_count;
        public long nominal_capacity_mah;
        public long design_capacity_mah;
        public byte health_percent;
        public byte mosfet_status;
        public int protection_status;
        public long balance_status;

        public BatteryData() {}
    }

    // ═══════════════════════════════════════════════════════════
    // CAPTEURS - Monitoring environnemental
    // ═══════════════════════════════════════════════════════════
    public static class SensorData {
        public float cabin_temperature;
        public float exterior_temperature;
        public float humidity;
        public int co2_level;
        public boolean door_open;

        public SensorData() {}
    }

    // ═══════════════════════════════════════════════════════════
    // CHAUFFAGE - Diesel water heater + Air heater
    // ═══════════════════════════════════════════════════════════
    public static class HeaterData {
        public boolean heater_on;
        public float target_air_temperature;
        public float actual_air_temperature;
        public float antifreeze_temperature;
        public byte fuel_level_percent;
        public int error_code;
        public boolean pump_active;
        public byte radiator_fan_speed;

        public HeaterData() {}
    }

    // ═══════════════════════════════════════════════════════════
    // ÉCLAIRAGE - LED strips (Interior + Exterior)
    // ═══════════════════════════════════════════════════════════
    public static class LedStrip {
        public boolean enabled;
        public byte current_mode;
        public int brightness;

        public LedStrip() {}
    }

    public static class LedData {
        public LedStrip leds_roof1;
        public LedStrip leds_roof2;
        public LedStrip leds_av;
        public LedStrip leds_ar;

        public LedData() {
            leds_roof1 = new LedStrip();
            leds_roof2 = new LedStrip();
            leds_av = new LedStrip();
            leds_ar = new LedStrip();
        }
    }

    // ═══════════════════════════════════════════════════════════
    // SYSTÈME - Status général & erreurs
    // ═══════════════════════════════════════════════════════════
    public static class SystemData {
        public long uptime;
        public boolean system_error;
        public long error_code;
        public MainErrorState errors;

        public SystemData() {}
    }

    // Main PCB error state
    public static class MainErrorState {
        // Add fields as needed for main_error_state_t
        // Placeholder for future expansion
        public MainErrorState() {}
    }

    // ═══════════════════════════════════════════════════════════
    // SLAVE PCB - État de la carte esclave
    // ═══════════════════════════════════════════════════════════
    public static class SlavePcbState {
        public long timestamp;
        public SystemCase current_case;
        public HoodState hood_state;
        public WaterTanksLevels tanks_levels;
        public SlaveErrorState error_state;
        public SlaveHealth system_health;

        public SlavePcbState() {
            tanks_levels = new WaterTanksLevels();
            error_state = new SlaveErrorState();
            system_health = new SlaveHealth();
        }
    }

    // ═══════════════════════════════════════════════════════════
    // VIDÉOPROJECTEUR - Motorisé (commandes BLE)
    // ═══════════════════════════════════════════════════════════
    public static class ProjectorData {
        public ProjectorState state;        // État du vidéoprojecteur (Retracted/Deploying/Deployed/Retracting)
        public boolean connected;           // Connecté via BLE
        public long last_update_time;       // Timestamp de la dernière mise à jour

        public ProjectorData() {
            state = ProjectorState.PROJECTOR_STATE_UNKNOWN;
            connected = false;
            last_update_time = 0;
        }
    }

    public enum SystemCase {
        CASE_RST, CASE_E1, CASE_E2, CASE_E3, CASE_E4,
        CASE_D1, CASE_D2, CASE_D3, CASE_D4,
        CASE_V1, CASE_V2, CASE_P1, CASE_MAX
    }

    public enum HoodState {
        HOOD_OFF, HOOD_ON
    }

    public static class WaterTankData {
        public float level_percentage;
        public float weight_kg;
        public float volume_liters;

        public WaterTankData() {}
    }

    public static class WaterTanksLevels {
        public WaterTankData tank_a;
        public WaterTankData tank_b;
        public WaterTankData tank_c;
        public WaterTankData tank_d;
        public WaterTankData tank_e;

        public WaterTanksLevels() {
            tank_a = new WaterTankData();
            tank_b = new WaterTankData();
            tank_c = new WaterTankData();
            tank_d = new WaterTankData();
            tank_e = new WaterTankData();
        }
    }

    public static class SlaveHealth {
        public boolean system_healthy;
        public long last_health_check;
        public long uptime_seconds;
        public long free_heap_size;
        public long min_free_heap_size;

        public SlaveHealth() {}
    }

    public static class SlaveErrorEvent {
        public long error_code;
        public ErrorSeverity severity;
        public ErrorCategory category;
        public long timestamp;
        public String module;
        public String description;
        public long data;

        public SlaveErrorEvent() {}
    }

    public enum ErrorSeverity {
        LOW, MEDIUM, HIGH, CRITICAL
    }

    public enum ErrorCategory {
        GENERAL, COMMUNICATION, DEVICE, STATE, SAFETY, OTHER1, OTHER2, OTHER3
    }

    public static class SlaveErrorStats {
        public long total_errors;
        public long[] errors_by_severity = new long[4];
        public long[] errors_by_category = new long[8];
        public long last_error_timestamp;
        public long last_error_code;

        public SlaveErrorStats() {}
    }

    public static class SlaveErrorState {
        public SlaveErrorStats error_stats;
        public SlaveErrorEvent[] last_errors = new SlaveErrorEvent[10]; // MAX_ERROR_HISTORY = 10

        public SlaveErrorState() {
            error_stats = new SlaveErrorStats();
            for (int i = 0; i < last_errors.length; i++) {
                last_errors[i] = new SlaveErrorEvent();
            }
        }
    }

    // ═══════════════════════════════════════════════════════════
    // Champs principaux
    // ═══════════════════════════════════════════════════════════
    public MpptData mppt;
    public AlternatorChargerData alternator_charger;
    public InverterChargerData inverter_charger;
    public BatteryData battery;
    public SensorData sensors;
    public HeaterData heater;
    public LedData leds;
    public SystemData system;
    public ProjectorData projector;
    public SlavePcbState slave_pcb;

    public VanState() {
        mppt = new MpptData();
        alternator_charger = new AlternatorChargerData();
        inverter_charger = new InverterChargerData();
        battery = new BatteryData();
        sensors = new SensorData();
        heater = new HeaterData();
        leds = new LedData();
        system = new SystemData();
        projector = new ProjectorData();
        slave_pcb = new SlavePcbState();
    }
}
