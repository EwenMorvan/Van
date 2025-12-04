package com.van.management.ui.tabs;

import android.util.Log;
import android.view.View;

import com.van.management.data.VanState;

/**
 * Gère l'onglet Heater avec le chauffage
 */
public class HeaterTab extends BaseTab {
    private static final String TAG = "HeaterTab";
    
    @Override
    protected void onInitialize() {
        Log.d(TAG, "Initialisation du HeaterTab");
        // TODO: Initialiser les vues et listeners
    }
    
    @Override
    public void updateUI(VanState vanState) {
        if (rootView == null || vanState == null) return;
        Log.d(TAG, "Mise à jour UI avec VanState");
        // TODO: Mettre à jour les informations de chauffage
    }
}
