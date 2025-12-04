package com.van.management.ui.tabs;

import android.util.Log;
import android.view.View;

import com.van.management.data.VanState;

/**
 * Gère l'onglet Water avec la gestion de l'eau
 */
public class WaterTab extends BaseTab {
    private static final String TAG = "WaterTab";
    
    @Override
    protected void onInitialize() {
        Log.d(TAG, "Initialisation du WaterTab");
        // TODO: Initialiser les vues et listeners
    }
    
    @Override
    public void updateUI(VanState vanState) {
        if (rootView == null || vanState == null) return;
        Log.d(TAG, "Mise à jour UI avec VanState");
        // TODO: Mettre à jour les informations d'eau
    }
}
