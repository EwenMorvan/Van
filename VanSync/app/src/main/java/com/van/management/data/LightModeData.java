package com.van.management.data;

import java.util.ArrayList;
import java.util.List;

/**
 * Classe pour stocker les données d'un mode de lumière
 * Utilisée pour la persistence et le chargement des modes personnalisés
 */
public class LightModeData {
    private int number;           // Numéro du mode (1-20 pour interior, 1-10 pour exterior)
    private String name;          // Nom du mode (ex: "Off", "White", "Rainbow")
    private List<Integer> colors; // Liste des couleurs pour le rendu visuel
    private boolean isEditable;   // true si le mode peut être modifié/supprimé
    private LedModeConfig ledConfig; // Configuration RGBW des strips pour l'envoi de commandes
    
    /**
     * Constructeur pour créer un nouveau mode
     */
    public LightModeData(int number, String name, List<Integer> colors, boolean isEditable) {
        this.number = number;
        this.name = name;
        this.colors = colors;
        this.isEditable = isEditable;
        this.ledConfig = null;
    }
    
    /**
     * Constructeur avec configuration LED
     */
    public LightModeData(int number, String name, List<Integer> colors, boolean isEditable, LedModeConfig ledConfig) {
        this.number = number;
        this.name = name;
        this.colors = colors;
        this.isEditable = isEditable;
        this.ledConfig = ledConfig;
    }
    
    /**
     * Constructeur avec configuration LED (génère les couleurs automatiquement)
     */
    public LightModeData(int number, String name, boolean isEditable, LedModeConfig ledConfig) {
        this.number = number;
        this.name = name;
        this.colors = ledConfig != null ? ledConfig.getSampledDisplayColors(10) : new ArrayList<>();
        this.isEditable = isEditable;
        this.ledConfig = ledConfig;
    }
    
    // Getters et Setters
    
    public int getNumber() {
        return number;
    }
    
    public void setNumber(int number) {
        this.number = number;
    }
    
    public String getName() {
        return name;
    }
    
    public void setName(String name) {
        this.name = name;
    }
    
    public List<Integer> getColors() {
        return colors;
    }
    
    public void setColors(List<Integer> colors) {
        this.colors = colors;
    }
    
    public boolean isEditable() {
        return isEditable;
    }
    
    public void setEditable(boolean editable) {
        isEditable = editable;
    }
    
    public LedModeConfig getLedConfig() {
        return ledConfig;
    }
    
    public void setLedConfig(LedModeConfig ledConfig) {
        this.ledConfig = ledConfig;
        // Mettre à jour les couleurs d'affichage
        if (ledConfig != null) {
            this.colors = ledConfig.getSampledDisplayColors(10);
        }
    }
    
    /**
     * Créer et envoyer la commande BLE pour activer ce mode
     */
    public VanCommand createVanCommand() {
        if (ledConfig != null) {
            return ledConfig.createStaticCommand();
        }
        return null;
    }
    
    /**
     * Vérifie si le mode est un placeholder (non défini par l'utilisateur)
     */
    public boolean isPlaceholder() {
        return name.equals("Click edit to add");
    }
    
    @Override
    public String toString() {
        return "LightModeData{" +
                "number=" + number +
                ", name='" + name + '\'' +
                ", colors=" + colors +
                ", isEditable=" + isEditable +
                '}';
    }
}
