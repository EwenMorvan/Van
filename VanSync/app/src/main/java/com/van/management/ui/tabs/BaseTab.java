package com.van.management.ui.tabs;

import android.view.View;
import com.van.management.data.VanCommand;
import com.van.management.data.VanState;

/**
 * Classe de base abstraite pour tous les tabs de l'application
 */
public abstract class BaseTab {
    
    /**
     * Interface pour envoyer des commandes BLE depuis les tabs
     */
    public interface CommandSender {
        boolean sendBinaryCommand(VanCommand command);
    }
    
    protected View rootView;
    protected boolean isInitialized = false;
    protected CommandSender commandSender;
    
    /**
     * Initialise le tab avec sa vue racine
     */
    public void initialize(View view) {
        if (isInitialized) {
            // Déjà initialisé, ne rien faire
            return;
        }
        this.rootView = view;
        onInitialize();
        isInitialized = true;
    }
    
    /**
     * Définit le callback pour envoyer des commandes
     */
    public void setCommandSender(CommandSender sender) {
        this.commandSender = sender;
    }
    
    /**
     * Vérifie si le tab a été initialisé
     */
    public boolean isInitialized() {
        return isInitialized;
    }
    
    /**
     * Méthode appelée lors de l'initialisation du tab
     * À implémenter par les classes filles pour setup les listeners, etc.
     */
    protected abstract void onInitialize();
    
    /**
     * Met à jour l'UI du tab avec les nouvelles données
     */
    public abstract void updateUI(VanState vanState);
    
    /**
     * Retourne la vue racine du tab
     */
    public View getRootView() {
        return rootView;
    }
    
    /**
     * Méthode appelée quand le tab devient visible
     */
    public void onTabSelected() {
        // Override si nécessaire
    }
    
    /**
     * Méthode appelée quand le tab n'est plus visible
     */
    public void onTabDeselected() {
        // Override si nécessaire
    }
}
