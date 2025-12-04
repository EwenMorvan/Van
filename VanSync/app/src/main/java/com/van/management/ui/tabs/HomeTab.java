package com.van.management.ui.tabs;

import android.util.Log;
import android.view.View;
import android.widget.TextView;

import com.van.management.R;
import com.van.management.data.VanState;
import com.van.management.ui.battery.BatteryGaugeView;

/**
 * Gère l'onglet Home avec les informations générales du van
 */
public class HomeTab extends BaseTab {
    private static final String TAG = "HomeTab";
    
    // Views
    private BatteryGaugeView batteryGauge;
    private TextView stateValue;
    private TextView powerValue;
    private TextView voltageValue;
    private TextView currentValue;
    
    @Override
    protected void onInitialize() {
        Log.d(TAG, "Initialisation du HomeTab");
        
        if (rootView == null) return;
        
        // Récupérer les références aux vues
        batteryGauge = rootView.findViewById(R.id.battery_gauge_bg);
        stateValue = rootView.findViewById(R.id.state_value);
        powerValue = rootView.findViewById(R.id.power_value);
        voltageValue = rootView.findViewById(R.id.voltage_value);
        currentValue = rootView.findViewById(R.id.current_value);
        
        if (batteryGauge == null || stateValue == null) {
            Log.e(TAG, "onInitialize: Certaines vues n'ont pas été trouvées");
        }
    }
    
    @Override
    public void updateUI(VanState vanState) {
        if (rootView == null || vanState == null) {
            Log.w(TAG, "updateUI: rootView ou vanState null");
            return;
        }
        
        // Si les vues ne sont pas initialisées, les récupérer
        if (batteryGauge == null || stateValue == null) {
            Log.w(TAG, "updateUI: Views non initialisées, récupération...");
            batteryGauge = rootView.findViewById(R.id.battery_gauge_bg);
            stateValue = rootView.findViewById(R.id.state_value);
            powerValue = rootView.findViewById(R.id.power_value);
            voltageValue = rootView.findViewById(R.id.voltage_value);
            currentValue = rootView.findViewById(R.id.current_value);
            
            if (batteryGauge == null || stateValue == null) {
                Log.e(TAG, "updateUI: Impossible de trouver les vues");
                return;
            }
        }

        Log.d(TAG, "updateUI: Mise à jour avec SOC=" + vanState.battery.soc_percent + "%");

        // Update battery gauge
        batteryGauge.setBatteryLevel(vanState.battery.soc_percent);

        // Calculate values
        float current_a = vanState.battery.current_ma / 1000.0f;
        float voltage_v = vanState.battery.voltage_mv / 1000.0f;
        float power_w = current_a * voltage_v;

        // Determine state
        String state;
        if (vanState.battery.current_ma > 100) {
            state = "⚡CHARGING";
        } else if (vanState.battery.current_ma < -100) {
            state = "🔋DISCHARGE";
        } else {
            state = "⏸️IDLE";
        }

        // Update UI
        stateValue.setText(state);
        powerValue.setText(String.format("%.1fW", power_w));
        voltageValue.setText(String.format("%.2fV", voltage_v));
        currentValue.setText(String.format("%.2fA", current_a));
    }
}
