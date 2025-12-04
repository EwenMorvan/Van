package com.van.management.data;

/**
 * Exemple de VanState statique pour tester l'affichage des flux d'énergie
 * Permet de tester facilement différents scénarios sans ESP32
 */
public class VanStateExample {
    
    /**
     * Scénario 1: Journée ensoleillée - Charge batterie + consommation
     * Solar: 350W, Alternator: 0W, Charger: 0W
     * Battery: +150W (charge), 12V: 100W, 220V: 100W
     */
    public static VanState createSunnyDayScenario() {
        VanState state = new VanState();
        
        // MPPT (Solar) - pas besoin de new, déjà initialisé dans constructeur
        state.mppt.solar_power_100_50 = 710.0f;  // MPPT 100/50
        state.mppt.solar_power_70_15 = 140.0f;   // MPPT 70/15
        state.mppt.battery_voltage_100_50 = 13.2f;
        state.mppt.battery_current_100_50 = 15.9f;
        state.mppt.temperature_100_50 = 38;
        state.mppt.state_100_50 = VanState.ChargeState.CHARGE_STATE_BULK; // Bulk charging
        state.mppt.battery_voltage_70_15 = 13.2f;
        state.mppt.battery_current_70_15 = 10.6f;
        state.mppt.temperature_70_15 = 35;
        state.mppt.state_70_15 = VanState.ChargeState.CHARGE_STATE_BULK;
        
        // Battery
        state.battery.voltage_mv = 13200;
        state.battery.current_ma = 11364; // +11.364A = +150W charge (350W solar - 200W consommation)
        state.battery.soc_percent = 75;
        state.battery.temp_sensor_count = 2;
        state.battery.temperatures_c[0] = 22;
        state.battery.temperatures_c[1] = 23;
        
        // Inverter/Charger (inverter mode, no AC input)
        state.inverter_charger.enabled = true;
        state.inverter_charger.ac_input_voltage = 0.0f;
        state.inverter_charger.ac_input_power = 0.0f;
        state.inverter_charger.ac_output_voltage = 230.0f;
        state.inverter_charger.ac_output_power = 500.0f;     // 220V output
        state.inverter_charger.battery_voltage = 13.2f;
        state.inverter_charger.battery_current = -7.6f;       // Décharge pour alimenter inverter
        state.inverter_charger.charger_state = VanState.ChargeState.CHARGE_STATE_OFF;
        
        // Alternator/Charger (moteur éteint)
        state.alternator_charger.state = VanState.ChargeState.CHARGE_STATE_OFF;
        state.alternator_charger.output_voltage = 0.0f;
        state.alternator_charger.output_current = 0.0f;
        
        return state;
    }
    
    /**
     * Scénario 2: Nuit - Décharge batterie
     * Solar: 0W, Alternator: 0W, Charger: 0W
     * Battery: -250W (décharge), 12V: 150W, 220V: 100W
     */
    public static VanState createNightScenario() {
        VanState state = new VanState();
        
        // MPPT (Pas de soleil)
        state.mppt.solar_power_100_50 = 0.0f;
        state.mppt.solar_power_70_15 = 0.0f;
        state.mppt.battery_voltage_100_50 = 12.6f;
        state.mppt.battery_current_100_50 = 0.0f;
        state.mppt.temperature_100_50 = 18;
        state.mppt.state_100_50 = VanState.ChargeState.CHARGE_STATE_OFF;
        state.mppt.battery_voltage_70_15 = 12.6f;
        state.mppt.battery_current_70_15 = 0.0f;
        state.mppt.temperature_70_15 = 18;
        state.mppt.state_70_15 = VanState.ChargeState.CHARGE_STATE_OFF;
        
        // Battery (décharge)
        state.battery.voltage_mv = 12600;
        state.battery.current_ma = -19841; // -19.841A = -250W décharge
        state.battery.soc_percent = 65;
        state.battery.temp_sensor_count = 2;
        state.battery.temperatures_c[0] = 20;
        state.battery.temperatures_c[1] = 20;
        
        // Inverter/Charger (inverter mode)
        state.inverter_charger.enabled = true;
        state.inverter_charger.ac_input_voltage = 0.0f;
        state.inverter_charger.ac_input_power = 0.0f;
        state.inverter_charger.ac_output_voltage = 230.0f;
        state.inverter_charger.ac_output_power = 100.0f;
        state.inverter_charger.battery_voltage = 12.6f;
        state.inverter_charger.battery_current = -7.9f;
        state.inverter_charger.charger_state = VanState.ChargeState.CHARGE_STATE_OFF;
        
        // Alternator/Charger (éteint)
        state.alternator_charger.state = VanState.ChargeState.CHARGE_STATE_OFF;
        state.alternator_charger.output_voltage = 0.0f;
        state.alternator_charger.output_current = 0.0f;
        
        return state;
    }
    
    /**
     * Scénario 3: Conduite - Alternateur charge
     * Solar: 120W, Alternator: 280W, Charger: 0W
     * Battery: +300W (charge), 12V: 80W, 220V: 20W
     */
    public static VanState createDrivingScenario() {
        VanState state = new VanState();
        
        // MPPT (un peu de soleil)
        state.mppt.solar_power_100_50 = 75.0f;
        state.mppt.solar_power_70_15 = 45.0f;
        state.mppt.battery_voltage_100_50 = 14.1f;
        state.mppt.battery_current_100_50 = 5.3f;
        state.mppt.temperature_100_50 = 28;
        state.mppt.state_100_50 = VanState.ChargeState.CHARGE_STATE_BULK;
        state.mppt.battery_voltage_70_15 = 14.1f;
        state.mppt.battery_current_70_15 = 3.2f;
        state.mppt.temperature_70_15 = 26;
        state.mppt.state_70_15 = VanState.ChargeState.CHARGE_STATE_BULK;
        
        // Battery (charge forte)
        state.battery.voltage_mv = 14100;
        state.battery.current_ma = 21277; // +21.277A = +300W charge
        state.battery.soc_percent = 82;
        state.battery.temp_sensor_count = 2;
        state.battery.temperatures_c[0] = 24;
        state.battery.temperatures_c[1] = 25;
        
        // Inverter/Charger (inverter mode, faible charge)
        state.inverter_charger.enabled = true;
        state.inverter_charger.ac_input_voltage = 0.0f;
        state.inverter_charger.ac_input_power = 0.0f;
        state.inverter_charger.ac_output_voltage = 230.0f;
        state.inverter_charger.ac_output_power = 20.0f;
        state.inverter_charger.battery_voltage = 14.1f;
        state.inverter_charger.battery_current = -1.4f;
        state.inverter_charger.charger_state = VanState.ChargeState.CHARGE_STATE_OFF;
        
        // Alternator/Charger (moteur allumé)
        state.alternator_charger.state = VanState.ChargeState.CHARGE_STATE_BULK;
        state.alternator_charger.output_voltage = 14.1f;
        state.alternator_charger.output_current = 19.9f;
        
        return state;
    }
    
    /**
     * Scénario 4: Secteur branché (passthrough)
     * Solar: 0W, Alternator: 0W, Charger: 400W
     * Battery: +50W (charge lente), 12V: 50W, 220V: 300W (passthrough direct)
     */
    public static VanState createMainsPowerScenario() {
        VanState state = new VanState();
        
        // MPPT (pas de soleil)
        state.mppt.solar_power_100_50 = 500.0f;
        state.mppt.solar_power_70_15 = 100.0f;
        state.mppt.battery_voltage_100_50 = 13.8f;
        state.mppt.battery_current_100_50 = 0.0f;
        state.mppt.temperature_100_50 = 22;
        state.mppt.state_100_50 = VanState.ChargeState.CHARGE_STATE_OFF;
        state.mppt.battery_voltage_70_15 = 13.8f;
        state.mppt.battery_current_70_15 = 0.0f;
        state.mppt.temperature_70_15 = 22;
        state.mppt.state_70_15 = VanState.ChargeState.CHARGE_STATE_OFF;
        
        // Battery
        state.battery.voltage_mv = 12000;
        state.battery.current_ma = 12000;
        state.battery.soc_percent = 95;
        state.battery.temp_sensor_count = 2;
        state.battery.temperatures_c[0] = 21;
        state.battery.temperatures_c[1] = 21;
        
        // Inverter/Charger (secteur branché - passthrough + charger mode)
        state.inverter_charger.enabled = true;
        state.inverter_charger.ac_input_voltage = 230.0f;
        state.inverter_charger.ac_input_power = 400.0f;      // 400W secteur entrant
        state.inverter_charger.ac_output_voltage = 230.0f;
        state.inverter_charger.ac_output_power = 100.0f;     // 300W en sortie (direct passthrough)
        state.inverter_charger.battery_voltage = 13.8f;
        state.inverter_charger.battery_current = 0.0f;       // Charge lente
        state.inverter_charger.charger_state = VanState.ChargeState.CHARGE_STATE_FLOAT;
        
        // Alternator/Charger (éteint)
        state.alternator_charger.state = VanState.ChargeState.CHARGE_STATE_OFF;
        state.alternator_charger.output_voltage = 12.0f;
        state.alternator_charger.output_current = 20.0f;
        
        return state;
    }
    
    /**
     * Scénario 5: Tout à fond!
     * Solar: 600W, Alternator: 400W, Charger: 0W
     * Battery: +600W (charge max), 12V: 200W, 220V: 200W
     */
    public static VanState createMaxPowerScenario() {
        VanState state = new VanState();
        
        // MPPT (plein soleil)
        state.mppt.solar_power_100_50 = 380.0f;
        state.mppt.solar_power_70_15 = 220.0f;
        state.mppt.battery_voltage_100_50 = 14.4f;
        state.mppt.battery_current_100_50 = 26.4f;
        state.mppt.temperature_100_50 = 45;
        state.mppt.state_100_50 = VanState.ChargeState.CHARGE_STATE_BULK;
        state.mppt.battery_voltage_70_15 = 14.4f;
        state.mppt.battery_current_70_15 = 15.3f;
        state.mppt.temperature_70_15 = 42;
        state.mppt.state_70_15 = VanState.ChargeState.CHARGE_STATE_BULK;
        
        // Battery (charge maximale)
        state.battery.voltage_mv = 14400;
        state.battery.current_ma = 41667; // +41.667A = +600W charge
        state.battery.soc_percent = 45;
        state.battery.temp_sensor_count = 2;
        state.battery.temperatures_c[0] = 28;
        state.battery.temperatures_c[1] = 28;
        
        // Inverter/Charger (inverter mode, forte charge)
        state.inverter_charger.enabled = true;
        state.inverter_charger.ac_input_voltage = 0.0f;
        state.inverter_charger.ac_input_power = 0.0f;
        state.inverter_charger.ac_output_voltage = 230.0f;
        state.inverter_charger.ac_output_power = 200.0f;
        state.inverter_charger.battery_voltage = 14.4f;
        state.inverter_charger.battery_current = -13.9f;
        state.inverter_charger.charger_state = VanState.ChargeState.CHARGE_STATE_OFF;
        
        // Alternator/Charger (moteur à fond)
        state.alternator_charger.state = VanState.ChargeState.CHARGE_STATE_BULK;
        state.alternator_charger.output_voltage = 14.4f;
        state.alternator_charger.output_current = 27.8f;
        
        return state;
    }
}
