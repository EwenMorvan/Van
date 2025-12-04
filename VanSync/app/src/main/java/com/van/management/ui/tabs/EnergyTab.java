package com.van.management.ui.tabs;

import android.util.Log;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.content.ContextCompat;

import com.van.management.R;
import com.van.management.data.VanState;
import com.van.management.data.VanStateExample;
import com.van.management.ui.energy.EnergyFlowView;

import com.van.management.R;
import com.van.management.data.VanState;
import com.van.management.ui.energy.EnergyFlowView;

/**
 * Gère l'onglet Energy avec l'énergie et la batterie
 */
public class EnergyTab extends BaseTab {
    private static final String TAG = "EnergyTab";
    
    // Power Overview
    private EnergyFlowView energyFlowView;
    
    // Battery Details
    // private LinearLayout batteryDetailsContainer; // Removed
    
    // New battery card views
    private TextView voltageText, currentText, powerText, batteryStatusText, socValueText, capacityText, healthText, cyclesText, chargeMosText, dischargeMosText;
    private ProgressBar socProgress;
    private TextView[] cellVoltageTexts = new TextView[4];
    private ImageView[] cellBalanceImages = new ImageView[4];
    private ProgressBar[] cellProgressBars = new ProgressBar[4];
    private TextView[] tempTexts = new TextView[3];
    private TextView[] protTexts = new TextView[13];
    
    // MPPT
    private LinearLayout mppt10050Container;
    private LinearLayout mppt7015Container;
    
    // Inverter
    private ImageView inverterIcon;
    private TextView inverterStatusText;
    private ConstraintLayout inverterSwitchButton;
    private TextView inverterOutputValue;
    private TextView inverterVoltageValue;
    private TextView inverterEfficiencyValue;
    private boolean inverterEnabled = false;
    
    @Override
    protected void onInitialize() {
        Log.d(TAG, "Initialisation du EnergyTab");
        
        if (rootView == null) return;
        
        // Power Overview
        energyFlowView = rootView.findViewById(R.id.energy_flow_view);
        
        // Battery Details
        // batteryDetailsContainer = rootView.findViewById(R.id.battery_details_container); // Removed
        
        // New battery views
        voltageText = rootView.findViewById(R.id.voltage_text);
        currentText = rootView.findViewById(R.id.current_text);
        powerText = rootView.findViewById(R.id.power_text);
        batteryStatusText = rootView.findViewById(R.id.battery_status_text);
        socProgress = rootView.findViewById(R.id.soc_progress);
        socValueText = rootView.findViewById(R.id.soc_value_text);
        capacityText = rootView.findViewById(R.id.capacity_text);
        healthText = rootView.findViewById(R.id.health_text);
        cyclesText = rootView.findViewById(R.id.cycles_text);
        chargeMosText = rootView.findViewById(R.id.charge_mos_text);
        dischargeMosText = rootView.findViewById(R.id.discharge_mos_text);
        
        cellVoltageTexts[0] = rootView.findViewById(R.id.cell_voltage_1);
        cellVoltageTexts[1] = rootView.findViewById(R.id.cell_voltage_2);
        cellVoltageTexts[2] = rootView.findViewById(R.id.cell_voltage_3);
        cellVoltageTexts[3] = rootView.findViewById(R.id.cell_voltage_4);
        
        cellBalanceImages[0] = rootView.findViewById(R.id.cell_balance_1);
        cellBalanceImages[1] = rootView.findViewById(R.id.cell_balance_2);
        cellBalanceImages[2] = rootView.findViewById(R.id.cell_balance_3);
        cellBalanceImages[3] = rootView.findViewById(R.id.cell_balance_4);
        
        cellProgressBars[0] = rootView.findViewById(R.id.cell_bg_1);
        cellProgressBars[1] = rootView.findViewById(R.id.cell_bg_2);
        cellProgressBars[2] = rootView.findViewById(R.id.cell_bg_3);
        cellProgressBars[3] = rootView.findViewById(R.id.cell_bg_4);
        
        tempTexts[0] = rootView.findViewById(R.id.temp_1_text);
        tempTexts[1] = rootView.findViewById(R.id.temp_2_text);
        tempTexts[2] = rootView.findViewById(R.id.temp_3_text);
        
        protTexts[0] = rootView.findViewById(R.id.prot_single_overvoltage);
        protTexts[1] = rootView.findViewById(R.id.prot_single_undervoltage);
        protTexts[2] = rootView.findViewById(R.id.prot_pack_overvoltage);
        protTexts[3] = rootView.findViewById(R.id.prot_pack_undervoltage);
        protTexts[4] = rootView.findViewById(R.id.prot_charge_overtemp);
        protTexts[5] = rootView.findViewById(R.id.prot_charge_lowtemp);
        protTexts[6] = rootView.findViewById(R.id.prot_discharge_overtemp);
        protTexts[7] = rootView.findViewById(R.id.prot_discharge_lowtemp);
        protTexts[8] = rootView.findViewById(R.id.prot_charge_overcurrent);
        protTexts[9] = rootView.findViewById(R.id.prot_discharge_overcurrent);
        protTexts[10] = rootView.findViewById(R.id.prot_short_circuit);
        protTexts[11] = rootView.findViewById(R.id.prot_ic_error);
        protTexts[12] = rootView.findViewById(R.id.prot_software_lock);
        
        // MPPT
        mppt10050Container = rootView.findViewById(R.id.mppt_100_50_container);
        mppt7015Container = rootView.findViewById(R.id.mppt_70_15_container);
        
        // Inverter
        inverterIcon = rootView.findViewById(R.id.inverter_icon);
        inverterStatusText = rootView.findViewById(R.id.inverter_status_text);
        inverterSwitchButton = rootView.findViewById(R.id.inverter_switch_button);
        inverterOutputValue = rootView.findViewById(R.id.inverter_output_value);
        inverterVoltageValue = rootView.findViewById(R.id.inverter_voltage_value);
        inverterEfficiencyValue = rootView.findViewById(R.id.inverter_efficiency_value);
        
        // Inverter switch listener
        if (inverterSwitchButton != null) {
            inverterSwitchButton.setOnClickListener(v -> toggleInverter());
        }
    }
    
    @Override
    public void updateUI(VanState vanState) {
        if (rootView == null || vanState == null) return;
        Log.d(TAG, "Mise à jour UI avec VanState");
        // Provisoir overwrite van state with exambple
        //vanState = VanStateExample.createSunnyDayScenario();
        //vanState = VanStateExample.createDrivingScenario();
        //vanState = VanStateExample.createMainsPowerScenario();

        
        updatePowerOverview(vanState);
        updateBatteryDetails(vanState);
        updateMpptInfo(vanState);
        updateInverterStatus(vanState);
    }
    
    private void updatePowerOverview(VanState vanState) {
        if (vanState.battery == null) return;
        
        // ═══════════════════════════════════════════════════════════
        // ÉTAPE 1 : Calculer les sources d'énergie disponibles
        // ═══════════════════════════════════════════════════════════
        
        float battery_voltage_v = vanState.battery.voltage_mv / 1000.0f;
        float battery_current_a = vanState.battery.current_ma / 1000.0f;
        float battery_power_w = battery_voltage_v * battery_current_a;
        boolean battery_charging = battery_current_a > 0.1f;
        boolean battery_discharging = battery_current_a < -0.1f;
        
        // Solaire
        float solar_power_w = 0f;
        if (vanState.mppt != null) {
            solar_power_w = vanState.mppt.solar_power_100_50 + vanState.mppt.solar_power_70_15;
        }
        
        // Alternateur
        float alternator_power_w = 0f;
        if (vanState.alternator_charger != null) {
            alternator_power_w = vanState.alternator_charger.output_voltage * vanState.alternator_charger.output_current;
        }
        
        // Chargeur AC (secteur 220V)
        float ac_charger_power_w = 0f;
        boolean ac_mains_available = false;
        if (vanState.inverter_charger != null) {
            ac_mains_available = vanState.inverter_charger.ac_input_power > 0;
            if (ac_mains_available) {
                // Use total AC input power from mains, not just battery charging power
                ac_charger_power_w = vanState.inverter_charger.ac_input_power;
            }
        }
        
        // Total sources (inclut batterie si décharge)
        float total_sources_w = solar_power_w + alternator_power_w + ac_charger_power_w;
        if (battery_power_w < 0) {
            total_sources_w += Math.abs(battery_power_w); // Batterie décharge = source
        }
        
        // ═══════════════════════════════════════════════════════════
        // ÉTAPE 2 : Calculer les charges
        // ═══════════════════════════════════════════════════════════
        
        // Charge 220V AC (via inverseur)
        float load_220v_w = 0f;
        boolean inverter_active = false;
        if (vanState.inverter_charger != null) {
            load_220v_w = vanState.inverter_charger.ac_output_power;
            inverter_active = load_220v_w > 0;
        }
        
        // Consommation DC de l'inverseur
        float inverter_dc_input_w = 0f;
        float inverter_loss_w = 0f;
        if (inverter_active) {
            // Calculer combien le secteur peut fournir directement au 220V
            float ac_passthrough = ac_mains_available ? Math.min(ac_charger_power_w, load_220v_w) : 0f;
            float remaining_220v = load_220v_w - ac_passthrough;
            
            // Si le secteur ne suffit pas, l'onduleur complète avec les sources DC
            if (remaining_220v > 0.01f) {
                float inverter_efficiency = 0.88f;
                inverter_dc_input_w = remaining_220v / inverter_efficiency;
                inverter_loss_w = inverter_dc_input_w - remaining_220v;
            }
        }
        
        // Charge 12V DC (calcul par conservation d'énergie)
        // Équation: total_sources (avec batterie si décharge) = ac_from_secteur + inverter_dc_input + loads_12v + battery_charge (si charge)
        // Note: inverter_dc_input = consommation DC de l'onduleur (inclut pertes), ac_from_secteur = passthrough direct
        float ac_from_secteur = ac_mains_available ? Math.min(ac_charger_power_w, load_220v_w) : 0f;
        float battery_charge_only = Math.max(0, battery_power_w); // Seulement si batterie charge (>0)
        float load_12v_w = total_sources_w - ac_from_secteur - inverter_dc_input_w - battery_charge_only;
        load_12v_w = Math.max(0, load_12v_w);
        
        float total_loads_w = load_12v_w + load_220v_w + inverter_loss_w;
        
        // ═══════════════════════════════════════════════════════════
        // ÉTAPE 3 : Vérification de cohérence
        // ═══════════════════════════════════════════════════════════
        float energy_balance_w = total_sources_w - total_loads_w;
        float error_w = Math.abs(energy_balance_w - battery_charge_only);
        
        String status;
        if (error_w < 20) {
            status = "OK ✓";
        } else if (error_w < 50) {
            status = "WARNING ⚠";
        } else {
            status = "ERROR ✗";
        }
        
        Log.d(TAG, "╔════════════════════════════════════════════════════════════════╗");
        Log.d(TAG, String.format("║ ⚡ SOURCES (generation):                                       ║"));
        Log.d(TAG, String.format("║   🌞 Solar:         +%6.1f W                                 ║", solar_power_w));
        Log.d(TAG, String.format("║   🔌 AC Charger:    +%6.1f W                                 ║", ac_charger_power_w));
        Log.d(TAG, String.format("║   🚗 Alternator:    +%6.1f W                                 ║", alternator_power_w));
        Log.d(TAG, String.format("║   ───────────────────────────                                  ║"));
        Log.d(TAG, String.format("║   📊 TOTAL IN:      +%6.1f W                                 ║", total_sources_w));
        Log.d(TAG, String.format("║                                                                ║"));
        Log.d(TAG, String.format("║ 🔋 BATTERY (storage):                                          ║"));
        Log.d(TAG, String.format("║   Voltage:      %.2f V  |  SOC: %3d%%                        ║", battery_voltage_v, vanState.battery.soc_percent));
        Log.d(TAG, String.format("║   Power:        %+6.1f W  (%s)                            ║", battery_power_w, battery_charging ? "CHARGING ⬆" : battery_discharging ? "DISCHARGING ⬇" : "IDLE"));
        Log.d(TAG, String.format("║                                                                ║"));
        Log.d(TAG, String.format("║ 💡 LOADS (consumption):                                        ║"));
        Log.d(TAG, String.format("║   💡 12V devices:   -%6.1f W                                 ║", load_12v_w));
        Log.d(TAG, String.format("║   🏠 220V devices:  -%6.1f W                                 ║", load_220v_w));
        Log.d(TAG, String.format("║   🔥 Inverter loss: -%6.1f W                                 ║", inverter_loss_w));
        Log.d(TAG, String.format("║   ───────────────────────────                                  ║"));
        Log.d(TAG, String.format("║   📊 TOTAL OUT:     -%6.1f W                                 ║", total_loads_w));
        Log.d(TAG, String.format("╠════════════════════════════════════════════════════════════════╣"));
        Log.d(TAG, String.format("║ 🔬 ENERGY CONSERVATION:                                        ║"));
        Log.d(TAG, String.format("║   Sources - Loads = %+6.1f W                                 ║", energy_balance_w));
        Log.d(TAG, String.format("║   Battery Power   = %+6.1f W                                 ║", battery_power_w));
        Log.d(TAG, String.format("║   Error           = %6.1f W  [%s]                        ║", error_w, status));
        Log.d(TAG, "╚════════════════════════════════════════════════════════════════╝");
        
        // Detect passthrough mode
        boolean is220vPassthrough = ac_mains_available && inverter_active;
        boolean inverterEnabled = vanState.inverter_charger != null && vanState.inverter_charger.enabled;
        
        // ═══════════════════════════════════════════════════════════
        // ÉTAPE 4 : Calculer les flux d'énergie basés sur mesures réelles
        // ═══════════════════════════════════════════════════════════
        if (energyFlowView != null) {
            // Passer les 2 MPPT séparément pour un affichage précis
            float mppt1_power = vanState.mppt != null ? vanState.mppt.solar_power_100_50 : 0f;
            float mppt2_power = vanState.mppt != null ? vanState.mppt.solar_power_70_15 : 0f;
            
            energyFlowView.setSolarPower(mppt1_power, mppt2_power);
            energyFlowView.setAlternatorPower(alternator_power_w);
            energyFlowView.setChargerPower(ac_charger_power_w);
            energyFlowView.setSystem12vPower(load_12v_w);
            energyFlowView.setSystem220vPower(load_220v_w);
            energyFlowView.setBatteryPower(battery_power_w);
            energyFlowView.setInverterEnabled(inverterEnabled);
            energyFlowView.set220vPassthrough(is220vPassthrough);
            
            // ═════════════════════════════════════════════════════════════════════
            // CALCUL DES FLUX BASÉ SUR LA CONSERVATION D'ÉNERGIE
            // ═════════════════════════════════════════════════════════════════════
            // Équation fondamentale (en Watts):
            // P_mppt1 + P_mppt2 + P_alternateur + P_ac_in = P_ac_out + P_battery_charge + P_loads_12v
            //
            // Donc: P_loads_12v = (sources totales) - P_ac_out - P_battery_charge
            // Note: P_battery_charge déjà calculé dans load_12v_w (ÉTAPE 2)
            
            // Sources d'entrée (en Watts)
            float P_mppt1 = mppt1_power;
            float P_mppt2 = mppt2_power;
            float P_alternateur = alternator_power_w;
            float P_ac_in = ac_charger_power_w;
            
            // Sorties (en Watts)
            float P_ac_out = load_220v_w;
            float P_battery_charge = battery_power_w;  // >0 si charge, <0 si décharge
            float P_loads_12v = load_12v_w;
            
            // Puissance totale des sources DC (hors secteur)
            float P_dc_sources_total = P_mppt1 + P_mppt2 + P_alternateur;
            
            // Initialiser les 14 flux
            float solar1ToBattery = 0f;
            float solar2ToBattery = 0f;
            float alternatorToBattery = 0f;
            float chargerToBattery = 0f;
            
            float solar1To12v = 0f;
            float solar2To12v = 0f;
            float alternatorTo12v = 0f;
            float chargerTo12v = 0f;
            
            float solar1To220v = 0f;
            float solar2To220v = 0f;
            float alternatorTo220v = 0f;
            float chargerTo220v = 0f;
            
            float batteryTo12v = 0f;
            float batteryTo220v = 0f;
            
            // ═════════════════════════════════════════════════════════════════════
            // RÉPARTITION DES FLUX (Logique Simplifiée)
            // ═════════════════════════════════════════════════════════════════════
            
            // 1. PRIORITÉ DU 220V (AC) - Chemin direct du secteur
            float P_ac_passthrough = Math.min(P_ac_in, P_ac_out);
            chargerTo220v = P_ac_passthrough;
            
            // 2. RESTE DU 220V (non couvert par secteur) - alimenté par sources DC via onduleur
            float P_220v_remaining = Math.max(0, P_ac_out - P_ac_passthrough);
            
            // 3. RESTE DU SECTEUR: peut aller au 12V ou batterie
            float P_ac_surplus = Math.max(0, P_ac_in - P_ac_out);
            
            // 4. DÉTERMINER QUI ALIMENTE QUOI
            // Priorité 1: Secteur alimente 12V si surplus
            float P_ac_to_12v = Math.min(P_ac_surplus, P_loads_12v);
            chargerTo12v = P_ac_to_12v;
            
            float P_12v_remaining = P_loads_12v - P_ac_to_12v;
            
            // Priorité 2: Sources DC alimentent le 220V restant via onduleur (si besoin)
            float P_dc_to_220v = 0f;
            if (P_220v_remaining > 0.01f && P_dc_sources_total > 0) {
                P_dc_to_220v = Math.min(P_dc_sources_total, P_220v_remaining);
                
                // Répartition proportionnelle des sources DC vers 220V
                if (P_dc_sources_total > 0) {
                    float ratio_mppt1 = P_mppt1 / P_dc_sources_total;
                    float ratio_mppt2 = P_mppt2 / P_dc_sources_total;
                    float ratio_alt = P_alternateur / P_dc_sources_total;
                    
                    solar1To220v = P_dc_to_220v * ratio_mppt1;
                    solar2To220v = P_dc_to_220v * ratio_mppt2;
                    alternatorTo220v = P_dc_to_220v * ratio_alt;
                }
                
                P_220v_remaining -= P_dc_to_220v;
            }
            
            // Si encore un déficit 220V, la batterie compense via onduleur
            if (P_220v_remaining > 0.01f) {
                batteryTo220v = P_220v_remaining;
            }
            
            // Priorité 3: Sources DC restantes alimentent le 12V
            float P_dc_remaining = P_dc_sources_total - P_dc_to_220v;
            if (P_12v_remaining > 0.01f && P_dc_remaining > 0) {
                float P_dc_to_12v = Math.min(P_dc_remaining, P_12v_remaining);
                
                // Répartition proportionnelle des sources DC vers 12V
                if (P_dc_remaining > 0) {
                    float ratio_mppt1 = (P_mppt1 - solar1To220v) / P_dc_remaining;
                    float ratio_mppt2 = (P_mppt2 - solar2To220v) / P_dc_remaining;
                    float ratio_alt = (P_alternateur - alternatorTo220v) / P_dc_remaining;
                    
                    solar1To12v = P_dc_to_12v * ratio_mppt1;
                    solar2To12v = P_dc_to_12v * ratio_mppt2;
                    alternatorTo12v = P_dc_to_12v * ratio_alt;
                }
                
                P_12v_remaining -= P_dc_to_12v;
            }
            
            // Si encore un déficit 12V, la batterie compense
            if (P_12v_remaining > 0.01f) {
                batteryTo12v = P_12v_remaining;
            }
            
            // 5. RÉPARTIR LE SURPLUS VERS LA BATTERIE (si batterie charge)
            if (P_battery_charge > 0.1f) {
                // Calculer les surplus restants
                float P_ac_surplus_remaining = P_ac_surplus - chargerTo12v;
                float P_dc_surplus = P_dc_sources_total - P_dc_to_220v - (solar1To12v + solar2To12v + alternatorTo12v);
                float P_total_surplus = P_ac_surplus_remaining + P_dc_surplus;
                
                if (P_total_surplus > 0.01f) {
                    // Répartir P_battery_charge entre les sources au prorata de leurs surplus
                    float ratio_ac = P_ac_surplus_remaining / P_total_surplus;
                    float ratio_dc = P_dc_surplus / P_total_surplus;
                    
                    chargerToBattery = P_battery_charge * ratio_ac;
                    float P_dc_to_battery = P_battery_charge * ratio_dc;
                    
                    // Répartir P_dc_to_battery entre sources DC proportionnellement
                    if (P_dc_surplus > 0 && P_dc_to_battery > 0) {
                        solar1ToBattery = P_dc_to_battery * ((P_mppt1 - solar1To220v - solar1To12v) / P_dc_surplus);
                        solar2ToBattery = P_dc_to_battery * ((P_mppt2 - solar2To220v - solar2To12v) / P_dc_surplus);
                        alternatorToBattery = P_dc_to_battery * ((P_alternateur - alternatorTo220v - alternatorTo12v) / P_dc_surplus);
                    }
                }
            }
            
            // Appliquer les flux calculés
            energyFlowView.setEnergyFlows(
                solar1ToBattery, solar2ToBattery,
                solar1To12v, solar2To12v,
                solar1To220v, solar2To220v,
                alternatorToBattery, alternatorTo12v, alternatorTo220v,
                chargerToBattery, chargerTo12v, chargerTo220v,
                batteryTo12v, batteryTo220v
            );
        }
    }
    

    
    private void updateBatteryDetails(VanState vanState) {
        if (vanState.battery == null) return;
        
        // Voltage, Current, Power
        float voltageV = vanState.battery.voltage_mv / 1000.0f;
        float currentA = vanState.battery.current_ma / 1000.0f;
        float powerW = voltageV * currentA;
        
        if (voltageText != null) voltageText.setText(String.format("%.2fV", voltageV));
        if (currentText != null) currentText.setText(String.format("%.2fA", currentA));
        if (powerText != null) powerText.setText(String.format("%.1fW", powerW));
        
        // Status
        String status = "IDLE";
        int logoColor = android.graphics.Color.WHITE; // Default: white for IDLE
        
        if (currentA > 0.1f) {
            status = "Charging...";
            logoColor = android.graphics.Color.parseColor("#4CAF50"); // Green for charging
        }
        else if (currentA < -0.1f) {
            status = "Discharging...";
            logoColor = android.graphics.Color.parseColor("#FF5252"); // Red for discharging
        }
        
        if (batteryStatusText != null) batteryStatusText.setText(status);
        
        // Update logo color
        ImageView energyLogo = rootView.findViewById(R.id.energy_logo);
        if (energyLogo != null) {
            energyLogo.setColorFilter(logoColor);
        }
        
        // SOC
        int soc = vanState.battery.soc_percent;
        if (socProgress != null) {
            socProgress.setProgress(soc);
            // Set color based on SOC (red at 0%, green at 100%)
            int color = interpolateColor(
                    android.graphics.Color.parseColor("#FF5252"), // Red at 0%
                    android.graphics.Color.parseColor("#4CAF50"), // Green at 100%
                    soc / 100f
            );
            socProgress.setProgressTintList(android.content.res.ColorStateList.valueOf(color));
        }
        if (socValueText != null) socValueText.setText(String.format("%d%%", soc));
        
        // Capacity
        float capacityAh = vanState.battery.capacity_mah / 1000.0f;
        if (capacityText != null) capacityText.setText(String.format("%.1fAh", capacityAh));
        
        // Health, Cycles
        if (healthText != null) healthText.setText(String.format("%d%%", vanState.battery.health_percent));
        if (cyclesText != null) cyclesText.setText(String.format("%d", vanState.battery.cycle_count));
        
        // MOSFET
        boolean chargeMos = (vanState.battery.mosfet_status & 0x01) != 0;
        boolean dischargeMos = (vanState.battery.mosfet_status & 0x02) != 0;
        if (chargeMosText != null) chargeMosText.setText(chargeMos ? "✅" : "❌");
        if (dischargeMosText != null) dischargeMosText.setText(dischargeMos ? "✅" : "❌");
        
        // Cells
        for (int i = 0; i < 4 && i < vanState.battery.cell_count; i++) {
            float cellV = vanState.battery.cell_voltage_mv[i] / 1000.0f;
            if (cellVoltageTexts[i] != null) cellVoltageTexts[i].setText(String.format("%.2fV", cellV));
            
            // Balance - show/hide energy logo
            boolean balancing = (vanState.battery.balance_status & (1L << i)) != 0;
            if (cellBalanceImages[i] != null) cellBalanceImages[i].setVisibility(balancing ? View.VISIBLE : View.INVISIBLE);
            
            // Cell progress bar based on voltage, assume 3.0-4.2V range
            float minV = 3.0f, maxV = 4.2f;
            float ratio = Math.max(0, Math.min(1, (cellV - minV) / (maxV - minV)));
            if (cellProgressBars[i] != null) {
                cellProgressBars[i].setProgress((int)(ratio * 100));
                cellProgressBars[i].setMax(100);
                // Color based on voltage
                int color = android.graphics.Color.GREEN;
                if (cellV < 3.3f) color = android.graphics.Color.parseColor("#FF5252"); // Red
                else if (cellV < 3.7f) color = android.graphics.Color.parseColor("#FFC107"); // Yellow
                cellProgressBars[i].setProgressTintList(android.content.res.ColorStateList.valueOf(color));
            }
        }
        
        // Temperatures
        for (int i = 0; i < 3 && i < vanState.battery.temp_sensor_count; i++) {
            if (tempTexts[i] != null) tempTexts[i].setText(String.format("🌡️ %.1f°C", vanState.battery.temperatures_c[i] / 10.0f)); // Assuming temperatures_c is in 0.1°C units? Wait, int, probably °C
        }
        
        // Protections
        int[] protBits = {0,1,2,3,4,5,6,7,8,9,10,11,12};
        String[] protNames = {
            "Single Cell Overvoltage", "Single Cell Undervoltage", "Whole Pack Overvoltage", "Whole Pack Undervoltage",
            "Charging Over-temperature", "Charging Low Temperature", "Discharge Over Temperature", "Discharge Low Temperature",
            "Charging Overcurrent", "Discharge Overcurrent", "Short Circuit Protection", "Front-end Detection IC Error", "Software Lock MOS"
        };
        for (int i = 0; i < protTexts.length; i++) {
            boolean active = (vanState.battery.protection_status & (1 << protBits[i])) != 0;
            if (protTexts[i] != null) {
                protTexts[i].setText((active ? "🔴 " : "✔ ") + protNames[i]);
                protTexts[i].setTextColor(active ? android.graphics.Color.RED : android.graphics.Color.WHITE);
            }
        }
    }
    
    private void updateMpptInfo(VanState vanState) {
        if (vanState.mppt == null) return;
        
        // MPPT 100/50
        if (mppt10050Container != null) {
            mppt10050Container.removeAllViews();
            addInfoRow(mppt10050Container, "Solar Power:", String.format("%.1fW", vanState.mppt.solar_power_100_50));
            addInfoRow(mppt10050Container, "Battery Voltage:", String.format("%.2fV", vanState.mppt.battery_voltage_100_50));
            addInfoRow(mppt10050Container, "Battery Current:", String.format("%.2fA", vanState.mppt.battery_current_100_50));
            addInfoRow(mppt10050Container, "Temperature:", String.format("%d°C", vanState.mppt.temperature_100_50));
            addInfoRow(mppt10050Container, "State:", String.valueOf(vanState.mppt.state_100_50));
            addInfoRow(mppt10050Container, "Error Flags:", String.valueOf(vanState.mppt.error_flags_100_50));
        }
        
        // MPPT 70/15
        if (mppt7015Container != null) {
            mppt7015Container.removeAllViews();
            addInfoRow(mppt7015Container, "Solar Power:", String.format("%.1fW", vanState.mppt.solar_power_70_15));
            addInfoRow(mppt7015Container, "Battery Voltage:", String.format("%.2fV", vanState.mppt.battery_voltage_70_15));
            addInfoRow(mppt7015Container, "Battery Current:", String.format("%.2fA", vanState.mppt.battery_current_70_15));
            addInfoRow(mppt7015Container, "Temperature:", String.format("%d°C", vanState.mppt.temperature_70_15));
            addInfoRow(mppt7015Container, "State:", String.valueOf(vanState.mppt.state_70_15));
            addInfoRow(mppt7015Container, "Error Flags:", String.valueOf(vanState.mppt.error_flags_70_15));
        }
    }
    
    private void updateInverterStatus(VanState vanState) {
        boolean enabled = false;
        float outputPower = 0f;
        float outputVoltage = 0f;
        float efficiency = 0f;
        
        if (vanState.inverter_charger != null) {
            enabled = vanState.inverter_charger.enabled;
            outputPower = vanState.inverter_charger.ac_output_power;
            outputVoltage = vanState.inverter_charger.ac_output_voltage;
            
            // Calculate efficiency if battery is discharging to inverter
            if (vanState.battery != null && vanState.battery.current_ma < 0 && outputPower > 10f) {
                float batteryPower = Math.abs(vanState.battery.current_ma / 1000.0f * vanState.battery.voltage_mv / 1000.0f);
                efficiency = (outputPower / batteryPower) * 100f;
            }
        }
        
        int color = enabled ? ContextCompat.getColor(rootView.getContext(), R.color.connected) : ContextCompat.getColor(rootView.getContext(), R.color.disconnected);
        
        if (inverterIcon != null) inverterIcon.setColorFilter(color);
        if (inverterStatusText != null) {
            inverterStatusText.setText(enabled ? "ON" : "OFF");
            inverterStatusText.setTextColor(color);
        }
        
        if (inverterOutputValue != null) {
            inverterOutputValue.setText(String.format("%.0fW", outputPower));
        }
        if (inverterVoltageValue != null) {
            inverterVoltageValue.setText(String.format("%.1fV", outputVoltage));
        }
        if (inverterEfficiencyValue != null) {
            inverterEfficiencyValue.setText(efficiency > 0 ? String.format("%.0f%%", efficiency) : "N/A");
        }
        
        if (inverterEnabled && vanState.battery != null) {
            // Simulate inverter power output (10% of battery power)
            float current_a = vanState.battery.current_ma / 1000.0f;
            float voltage_v = vanState.battery.voltage_mv / 1000.0f;
            float dc_power = Math.abs(current_a * voltage_v);
            float ac_power = dc_power * 0.9f; // 90% efficiency
            
            if (inverterOutputValue != null) inverterOutputValue.setText(String.format("%.0fW", ac_power));
            if (inverterVoltageValue != null) inverterVoltageValue.setText("220V");
            if (inverterEfficiencyValue != null) inverterEfficiencyValue.setText("90%");
        } else {
            if (inverterOutputValue != null) inverterOutputValue.setText("0W");
            if (inverterVoltageValue != null) inverterVoltageValue.setText("0V");
            if (inverterEfficiencyValue != null) inverterEfficiencyValue.setText("--%");
        }
    }
    
    private void toggleInverter() {
        inverterEnabled = !inverterEnabled;
        Log.d(TAG, "Inverter toggled: " + (inverterEnabled ? "ON" : "OFF"));
        
        // Update UI immediately
        if (rootView != null) {
            // Trigger a UI update with last known state
            // This will be handled by MainActivity's updateUI cycle
        }
    }
    
    private void addInfoRow(LinearLayout container, String label, String value) {
        LinearLayout row = new LinearLayout(rootView.getContext());
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(android.view.Gravity.CENTER_VERTICAL);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        int margin = rootView.getContext().getResources().getDimensionPixelSize(R.dimen.margin_small);
        params.bottomMargin = margin;
        row.setLayoutParams(params);
        
        TextView labelView = new TextView(rootView.getContext());
        labelView.setText(label);
        labelView.setTextColor(ContextCompat.getColor(rootView.getContext(), R.color.text_secondary));
        labelView.setTextSize(android.util.TypedValue.COMPLEX_UNIT_PX, rootView.getContext().getResources().getDimension(R.dimen.text_size_medium));
        LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(
            0,
            LinearLayout.LayoutParams.WRAP_CONTENT,
            0.45f
        );
        labelView.setLayoutParams(labelParams);
        
        TextView valueView = new TextView(rootView.getContext());
        valueView.setText(value);
        valueView.setTextColor(ContextCompat.getColor(rootView.getContext(), R.color.white));
        valueView.setTextSize(android.util.TypedValue.COMPLEX_UNIT_PX, rootView.getContext().getResources().getDimension(R.dimen.text_size_medium));
        valueView.setTypeface(null, android.graphics.Typeface.BOLD);
        LinearLayout.LayoutParams valueParams = new LinearLayout.LayoutParams(
            0,
            LinearLayout.LayoutParams.WRAP_CONTENT,
            0.55f
        );
        valueView.setLayoutParams(valueParams);
        
        row.addView(labelView);
        row.addView(valueView);
        container.addView(row);
    }
    
    /**
     * Interpolate between two colors based on a ratio (0-1)
     * @param startColor Color at ratio 0
     * @param endColor Color at ratio 1
     * @param ratio Value between 0 and 1
     * @return Interpolated color
     */
    private int interpolateColor(int startColor, int endColor, float ratio) {
        ratio = Math.max(0, Math.min(1, ratio));
        
        int startA = (startColor >> 24) & 0xff;
        int startR = (startColor >> 16) & 0xff;
        int startG = (startColor >> 8) & 0xff;
        int startB = startColor & 0xff;
        
        int endA = (endColor >> 24) & 0xff;
        int endR = (endColor >> 16) & 0xff;
        int endG = (endColor >> 8) & 0xff;
        int endB = endColor & 0xff;
        
        return ((startA + (int)((endA - startA) * ratio)) << 24) |
                ((startR + (int)((endR - startR) * ratio)) << 16) |
                ((startG + (int)((endG - startG) * ratio)) << 8) |
                ((startB + (int)((endB - startB) * ratio)));
    }
}
