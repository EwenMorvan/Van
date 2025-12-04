package com.van.management.data;

import java.util.ArrayList;
import java.util.List;

/**
 * Configuration complète d'un mode LED avec les données des 2 strips
 * Utilisé pour sauvegarder et charger les modes personnalisés
 */
public class LedModeConfig {
    private LedStripData strip1Data;  // Strip 1 (120 LEDs)
    private LedStripData strip2Data;  // Strip 2 (120 LEDs) - peut être null si copie du strip 1
    private boolean copyStrip1;       // true si strip2 copie strip1
    
    /**
     * Constructeur par défaut (strips éteints)
     */
    public LedModeConfig() {
        strip1Data = new LedStripData();
        strip2Data = new LedStripData();
        copyStrip1 = false;
    }
    
    /**
     * Constructeur avec couleur uniforme
     * @param copyStrip1 true = 2 strips identiques, false = 1 seul strip
     */
    public LedModeConfig(int r, int g, int b, int w, boolean copyStrip1) {
        strip1Data = new LedStripData(r, g, b, w);
        this.copyStrip1 = copyStrip1;
        // Si copyStrip1 = true : les 2 strips sont identiques (strip2Data reste null, sera copié dynamiquement)
        // Si copyStrip1 = false : 1 seul strip (strip2Data reste null)
        this.strip2Data = null;
    }
    
    /**
     * Constructeur avec strips différents
     */
    public LedModeConfig(LedStripData strip1, LedStripData strip2, boolean copyStrip1) {
        this.strip1Data = strip1;
        this.strip2Data = copyStrip1 ? null : strip2;
        this.copyStrip1 = copyStrip1;
    }
    
    /**
     * Obtenir les données du strip 2 (retourne strip1 si copyStrip1 est vrai)
     */
    public LedStripData getStrip2DataEffective() {
        return copyStrip1 ? strip1Data : strip2Data;
    }
    
    /**
     * Créer une commande VanCommand statique pour ce mode
     * 
     * Logique :
     * - copyStrip1 = true : Les 2 strips identiques (target: ROOF_LED_ALL)
     * - copyStrip1 = false ET strip2Data != null : Les 2 strips différents (target: ROOF_LED_ALL)
     * - copyStrip1 = false ET strip2Data = null : Seulement strip1 (target: ROOF_LED1)
     */
    public VanCommand createStaticCommand() {
        LedData[] strip1Array = strip1Data.toLedDataArray();
        
        // Vérifier si le strip2 existe ET n'est pas vide
        boolean hasStrip2 = strip2Data != null && !strip2Data.isEmpty();
        
        // Déterminer quelle cible utiliser
        LedStaticCommand.StaticTarget target;
        LedData[] strip2Array;
        
        if (copyStrip1) {
            // Les 2 strips identiques (checkbox "Copy Strip 1" cochée)
            target = LedStaticCommand.StaticTarget.ROOF_LED_ALL;
            strip2Array = strip1Array; // Copie du strip 1
        } else if (hasStrip2) {
            // Les 2 strips différents (2 strips configurés séparément)
            target = LedStaticCommand.StaticTarget.ROOF_LED_ALL;
            strip2Array = strip2Data.toLedDataArray();
        } else {
            // Seulement strip1 (un seul strip configuré ou strip2 vide)
            target = LedStaticCommand.StaticTarget.ROOF_LED1;
            strip2Array = new LedStripData(0, 0, 0, 0).toLedDataArray(); // Strip 2 éteint
        }
        
        LedStaticCommand staticCmd = LedStaticCommand.createRoof(target, strip1Array, strip2Array);
        LedCommand ledCmd = LedCommand.createStatic(staticCmd);
        return VanCommand.createLedCommand(ledCmd);
    }
    
    /**
     * Vérifie si le mode utilise 2 strips différents
     */
    public boolean hasTwoStrips() {
        return !copyStrip1 && strip2Data != null && !strip2Data.isEmpty();
    }
    
    /**
     * Vérifie si le mode utilise seulement 1 strip
     */
    public boolean hasOneStrip() {
        return copyStrip1 || strip2Data == null || strip2Data.isEmpty();
    }
    
    /**
     * Obtenir toutes les couleurs d'affichage (240 LEDs total)
     * pour le preview circulaire
     */
    public List<Integer> getAllDisplayColors() {
        List<Integer> allColors = new ArrayList<>();
        allColors.addAll(strip1Data.getDisplayColors());
        allColors.addAll(getStrip2DataEffective().getDisplayColors());
        return allColors;
    }
    
    /**
     * Obtenir un échantillon de couleurs pour le preview (ex: 1 LED sur 10)
     */
    public List<Integer> getSampledDisplayColors(int sampleRate) {
        List<Integer> allColors = getAllDisplayColors();
        List<Integer> sampled = new ArrayList<>();
        for (int i = 0; i < allColors.size(); i += sampleRate) {
            sampled.add(allColors.get(i));
        }
        return sampled;
    }
    
    // Getters et Setters
    public LedStripData getStrip1Data() { return strip1Data; }
    public void setStrip1Data(LedStripData strip1Data) { this.strip1Data = strip1Data; }
    
    public LedStripData getStrip2Data() { return strip2Data; }
    public void setStrip2Data(LedStripData strip2Data) { this.strip2Data = strip2Data; }
    
    public boolean isCopyStrip1() { return copyStrip1; }
    public void setCopyStrip1(boolean copyStrip1) { this.copyStrip1 = copyStrip1; }
}
