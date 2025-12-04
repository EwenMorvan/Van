package com.van.management.data;

import java.util.ArrayList;
import java.util.List;

/**
 * Classe pour stocker les données RGBW d'un strip LED
 * Chaque strip contient 120 LEDs avec des valeurs R, G, B, W (0-255)
 */
public class LedStripData {
    public static final int LED_COUNT = 120;
    
    private int[] redValues;    // Valeurs R pour chaque LED (0-255)
    private int[] greenValues;  // Valeurs G pour chaque LED (0-255)
    private int[] blueValues;   // Valeurs B pour chaque LED (0-255)
    private int[] whiteValues;  // Valeurs W pour chaque LED (0-255)
    
    /**
     * Constructeur par défaut (toutes les LEDs éteintes)
     */
    public LedStripData() {
        redValues = new int[LED_COUNT];
        greenValues = new int[LED_COUNT];
        blueValues = new int[LED_COUNT];
        whiteValues = new int[LED_COUNT];
    }
    
    /**
     * Constructeur avec couleurs uniformes
     */
    public LedStripData(int r, int g, int b, int w) {
        this();
        for (int i = 0; i < LED_COUNT; i++) {
            redValues[i] = r;
            greenValues[i] = g;
            blueValues[i] = b;
            whiteValues[i] = w;
        }
    }
    
    /**
     * Constructeur avec un tableau de LedData
     */
    public LedStripData(List<Integer> displayColors) {
        this();
        // Convertir les couleurs d'affichage RGB en RGBW
        // Pour l'instant, on extrait juste RGB et on met W=0
        for (int i = 0; i < Math.min(LED_COUNT, displayColors.size()); i++) {
            int color = displayColors.get(i);
            redValues[i] = android.graphics.Color.red(color);
            greenValues[i] = android.graphics.Color.green(color);
            blueValues[i] = android.graphics.Color.blue(color);
            whiteValues[i] = 0; // Pas de canal W dans les couleurs RGB
        }
    }
    
    /**
     * Définir la couleur d'une LED spécifique
     */
    public void setLed(int index, int r, int g, int b, int w) {
        if (index >= 0 && index < LED_COUNT) {
            redValues[index] = r;
            greenValues[index] = g;
            blueValues[index] = b;
            whiteValues[index] = w;
        }
    }
    
    /**
     * Obtenir les valeurs RGBW d'une LED
     */
    public int[] getLed(int index) {
        if (index >= 0 && index < LED_COUNT) {
            return new int[]{redValues[index], greenValues[index], blueValues[index], whiteValues[index]};
        }
        return new int[]{0, 0, 0, 0};
    }
    
    /**
     * Convertir en tableau de LedData pour l'envoi BLE
     */
    public LedData[] toLedDataArray() {
        LedData[] ledArray = new LedData[LED_COUNT];
        for (int i = 0; i < LED_COUNT; i++) {
            ledArray[i] = new LedData(
                redValues[i],
                greenValues[i],
                blueValues[i],
                whiteValues[i],
                255 // Brightness par défaut
            );
        }
        return ledArray;
    }
    
    /**
     * Obtenir les couleurs d'affichage pour le preview circulaire
     * Mélange RGB + W pour l'affichage
     */
    public List<Integer> getDisplayColors() {
        List<Integer> colors = new ArrayList<>();
        for (int i = 0; i < LED_COUNT; i++) {
            // Mélanger W (4000K) avec RGB
            int r = Math.min(255, redValues[i] + (int)(whiteValues[i] * 1.0));
            int g = Math.min(255, greenValues[i] + (int)(whiteValues[i] * 0.917647));
            int b = Math.min(255, blueValues[i] + (int)(whiteValues[i] * 0.839216));
            colors.add(android.graphics.Color.rgb(r, g, b));
        }
        return colors;
    }
    
    /**
     * Vérifie si le strip est vide (toutes les LEDs à 0,0,0,0)
     * @return true si toutes les valeurs RGBW sont à 0
     */
    public boolean isEmpty() {
        for (int i = 0; i < LED_COUNT; i++) {
            if (redValues[i] != 0 || greenValues[i] != 0 || 
                blueValues[i] != 0 || whiteValues[i] != 0) {
                return false;
            }
        }
        return true;
    }
    
    // Getters
    public int[] getRedValues() { return redValues; }
    public int[] getGreenValues() { return greenValues; }
    public int[] getBlueValues() { return blueValues; }
    public int[] getWhiteValues() { return whiteValues; }
    
    // Setters
    public void setRedValues(int[] redValues) { this.redValues = redValues; }
    public void setGreenValues(int[] greenValues) { this.greenValues = greenValues; }
    public void setBlueValues(int[] blueValues) { this.blueValues = blueValues; }
    public void setWhiteValues(int[] whiteValues) { this.whiteValues = whiteValues; }
}
