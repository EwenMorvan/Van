package com.van.management.data;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.util.Log;

import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;

import java.lang.reflect.Type;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Repository pour gérer le chargement et la sauvegarde des modes de lumière
 * Gère les modes interior (max 20) et exterior (max 10)
 */
public class LightModesRepository {
    private static final String TAG = "LightModesRepository";
    private static final String PREFS_NAME = "light_modes_prefs";
    private static final String KEY_INTERIOR_MODES = "interior_modes";
    private static final String KEY_EXTERIOR_MODES = "exterior_modes";
    
    public static final int MAX_INTERIOR_MODES = 20;
    public static final int MAX_EXTERIOR_MODES = 10;
    public static final int VISIBLE_INTERIOR_MODES = 6;
    public static final int VISIBLE_EXTERIOR_MODES = 4;
    
    private final SharedPreferences prefs;
    private final Gson gson;
    
    private List<LightModeData> interiorModes;
    private List<LightModeData> exteriorModes;
    
    public LightModesRepository(Context context) {
        this.prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        this.gson = new Gson();
        
        // Charger les modes sauvegardés ou créer les modes par défaut
        loadModes();
    }
    
    /**
     * Charge les modes depuis SharedPreferences ou crée les modes par défaut
     */
    private void loadModes() {
        // Charger les modes interior
        String interiorJson = prefs.getString(KEY_INTERIOR_MODES, null);
        if (interiorJson != null) {
            Type type = new TypeToken<List<LightModeData>>() {}.getType();
            interiorModes = gson.fromJson(interiorJson, type);
            Log.d(TAG, "Modes interior chargés: " + interiorModes.size());
            
            // Si on a moins de 20 modes, compléter avec des placeholders
            ensureMaxSlots(interiorModes, MAX_INTERIOR_MODES);
        } else {
            interiorModes = createDefaultInteriorModes();
            Log.d(TAG, "Modes interior par défaut créés");
        }
        
        // Charger les modes exterior
        String exteriorJson = prefs.getString(KEY_EXTERIOR_MODES, null);
        if (exteriorJson != null) {
            Type type = new TypeToken<List<LightModeData>>() {}.getType();
            exteriorModes = gson.fromJson(exteriorJson, type);
            Log.d(TAG, "Modes exterior chargés: " + exteriorModes.size());
            
            // Si on a moins de 10 modes, compléter avec des placeholders
            ensureMaxSlots(exteriorModes, MAX_EXTERIOR_MODES);
        } else {
            exteriorModes = createDefaultExteriorModes();
            Log.d(TAG, "Modes exterior par défaut créés");
        }
    }
    
    /**
     * S'assure qu'une liste de modes a exactement maxSlots éléments
     * Complète avec des placeholders si nécessaire
     */
    private void ensureMaxSlots(List<LightModeData> modes, int maxSlots) {
        int currentSize = modes.size();
        if (currentSize < maxSlots) {
            Log.d(TAG, "Complétant " + (maxSlots - currentSize) + " slots manquants");
            for (int i = currentSize + 1; i <= maxSlots; i++) {
                modes.add(new LightModeData(i, "Click edit to add", 
                    Collections.singletonList(Color.GRAY), false));
            }
        }
    }
    
    /**
     * Crée les modes par défaut pour Interior Lights
     * 1. Off (non-éditable)
     * 2. White
     * 3-20. Placeholders "Click edit to add"
     */
    private List<LightModeData> createDefaultInteriorModes() {
        List<LightModeData> modes = new ArrayList<>();
        
        // Mode 1: Off (non-éditable) - Tout à 0
        LedModeConfig offConfig = new LedModeConfig(0, 0, 0, 0, true);
        modes.add(new LightModeData(1, "|| Off 🔒", false, offConfig));
        
        // Mode 2: White - (R:0, G:0, B:0, W:255) - 2 strips identiques (copyStrip1 = true)
        LedModeConfig whiteConfig = new LedModeConfig(0, 0, 0, 255, true);
        modes.add(new LightModeData(2, "|| White", true, whiteConfig));
        
        // Modes 3-20: Placeholders (tous les slots jusqu'au maximum)
        for (int i = 3; i <= MAX_INTERIOR_MODES; i++) {
            modes.add(new LightModeData(i, "Click edit to add", 
                Collections.singletonList(Color.GRAY), false));
        }
        
        return modes;
    }
    
    /**
     * Crée les modes par défaut pour Exterior Lights
     * 1. Off (non-éditable)
     * 2. White
     * 3-10. Placeholders "Click edit to add"
     */
    private List<LightModeData> createDefaultExteriorModes() {
        List<LightModeData> modes = new ArrayList<>();
        
        // Mode 1: Off (non-éditable) - Tout à 0
        LedModeConfig offConfig = new LedModeConfig(0, 0, 0, 0, true);
        modes.add(new LightModeData(1, "|| Off 🔒", false, offConfig));
        
        // Mode 2: White - (R:0, G:0, B:0, W:255) - 2 strips identiques (copyStrip1 = true)
        LedModeConfig whiteConfig = new LedModeConfig(0, 0, 0, 255, true);
        modes.add(new LightModeData(2, "|| White", true, whiteConfig));
        
        // Modes 3-10: Placeholders (tous les slots jusqu'au maximum)
        for (int i = 3; i <= MAX_EXTERIOR_MODES; i++) {
            modes.add(new LightModeData(i, "Click edit to add", 
                Collections.singletonList(Color.GRAY), false));
        }
        
        return modes;
    }
    
    /**
     * Retourne les modes interior visibles dans l'UI principale (6 premiers)
     */
    public List<LightModeData> getVisibleInteriorModes() {
        return interiorModes.subList(0, Math.min(VISIBLE_INTERIOR_MODES, interiorModes.size()));
    }
    
    /**
     * Retourne les modes exterior visibles dans l'UI principale (4 premiers)
     */
    public List<LightModeData> getVisibleExteriorModes() {
        return exteriorModes.subList(0, Math.min(VISIBLE_EXTERIOR_MODES, exteriorModes.size()));
    }
    
    /**
     * Retourne TOUS les modes interior (incluant placeholders) pour la popup de gestion
     */
    public List<LightModeData> getAllInteriorModesWithPlaceholders() {
        return new ArrayList<>(interiorModes);
    }
    
    /**
     * Retourne TOUS les modes exterior (incluant placeholders) pour la popup de gestion
     */
    public List<LightModeData> getAllExteriorModesWithPlaceholders() {
        return new ArrayList<>(exteriorModes);
    }
    
    /**
     * Retourne tous les modes interior (pour la popup)
     * Exclut les placeholders
     */
    public List<LightModeData> getAllInteriorModes() {
        List<LightModeData> nonPlaceholders = new ArrayList<>();
        for (LightModeData mode : interiorModes) {
            if (!mode.isPlaceholder()) {
                nonPlaceholders.add(mode);
            }
        }
        return nonPlaceholders;
    }
    
    /**
     * Retourne tous les modes exterior (pour la popup)
     * Exclut les placeholders
     */
    public List<LightModeData> getAllExteriorModes() {
        List<LightModeData> nonPlaceholders = new ArrayList<>();
        for (LightModeData mode : exteriorModes) {
            if (!mode.isPlaceholder()) {
                nonPlaceholders.add(mode);
            }
        }
        return nonPlaceholders;
    }
    
    /**
     * Trouve l'index réel d'un mode dans la liste complète par son nom
     * @return l'index dans interiorModes, ou -1 si non trouvé
     */
    public int findInteriorModeIndexByName(String name) {
        for (int i = 0; i < interiorModes.size(); i++) {
            if (interiorModes.get(i).getName().equals(name)) {
                return i;
            }
        }
        return -1;
    }
    
    /**
     * Trouve l'index réel d'un mode dans la liste complète par son nom
     * @return l'index dans exteriorModes, ou -1 si non trouvé
     */
    public int findExteriorModeIndexByName(String name) {
        for (int i = 0; i < exteriorModes.size(); i++) {
            if (exteriorModes.get(i).getName().equals(name)) {
                return i;
            }
        }
        return -1;
    }
    
    /**
     * Met à jour un mode interior spécifique
     */
    public void updateInteriorMode(int index, LightModeData updatedMode) {
        if (index >= 0 && index < interiorModes.size()) {
            interiorModes.set(index, updatedMode);
            saveInteriorModes(interiorModes);
            Log.d(TAG, "Mode interior " + index + " mis à jour");
        }
    }
    
    /**
     * Met à jour un mode exterior spécifique
     */
    public void updateExteriorMode(int index, LightModeData updatedMode) {
        if (index >= 0 && index < exteriorModes.size()) {
            exteriorModes.set(index, updatedMode);
            saveExteriorModes(exteriorModes);
            Log.d(TAG, "Mode exterior " + index + " mis à jour");
        }
    }
    
    /**
     * Échange deux modes interior entre eux (pour le swipe-to-reorder)
     * Ne peut pas déplacer le mode 0 (Off)
     */
    public boolean swapInteriorModes(int index1, int index2) {
        // Empêcher de déplacer le mode 0 (Off)
        if (index1 == 0 || index2 == 0) {
            Log.w(TAG, "Cannot move the 'Off' mode (index 0)");
            return false;
        }
        
        if (index1 < 0 || index1 >= interiorModes.size() || 
            index2 < 0 || index2 >= interiorModes.size()) {
            Log.w(TAG, "Invalid indices for swapping: " + index1 + ", " + index2);
            return false;
        }
        
        // Échanger les modes
        LightModeData temp = interiorModes.get(index1);
        interiorModes.set(index1, interiorModes.get(index2));
        interiorModes.set(index2, temp);
        
        // Mettre à jour les numéros (numéro = index + 1)
        interiorModes.get(index1).setNumber(index1 + 1);
        interiorModes.get(index2).setNumber(index2 + 1);
        
        // Sauvegarder
        saveInteriorModes(interiorModes);
        
        Log.d(TAG, "Swapped interior modes at indices " + index1 + " and " + index2);
        return true;
    }
    
    /**
     * Échange deux modes exterior entre eux (pour le swipe-to-reorder)
     * Ne peut pas déplacer le mode 0 (Off)
     */
    public boolean swapExteriorModes(int index1, int index2) {
        // Empêcher de déplacer le mode 0 (Off)
        if (index1 == 0 || index2 == 0) {
            Log.w(TAG, "Cannot move the 'Off' mode (index 0)");
            return false;
        }
        
        if (index1 < 0 || index1 >= exteriorModes.size() || 
            index2 < 0 || index2 >= exteriorModes.size()) {
            Log.w(TAG, "Invalid indices for swapping: " + index1 + ", " + index2);
            return false;
        }
        
        // Échanger les modes
        LightModeData temp = exteriorModes.get(index1);
        exteriorModes.set(index1, exteriorModes.get(index2));
        exteriorModes.set(index2, temp);
        
        // Mettre à jour les numéros (numéro = index + 1)
        exteriorModes.get(index1).setNumber(index1 + 1);
        exteriorModes.get(index2).setNumber(index2 + 1);
        
        // Sauvegarder
        saveExteriorModes(exteriorModes);
        
        Log.d(TAG, "Swapped exterior modes at indices " + index1 + " and " + index2);
        return true;
    }
    
    /**
     * Sauvegarde les modes interior
     */
    public void saveInteriorModes(List<LightModeData> modes) {
        this.interiorModes = new ArrayList<>(modes);
        String json = gson.toJson(modes);
        prefs.edit().putString(KEY_INTERIOR_MODES, json).apply();
        Log.d(TAG, "Modes interior sauvegardés: " + modes.size());
    }
    
    /**
     * Sauvegarde les modes exterior
     */
    public void saveExteriorModes(List<LightModeData> modes) {
        this.exteriorModes = new ArrayList<>(modes);
        String json = gson.toJson(modes);
        prefs.edit().putString(KEY_EXTERIOR_MODES, json).apply();
        Log.d(TAG, "Modes exterior sauvegardés: " + modes.size());
    }
    
    /**
     * Ajoute un nouveau mode interior
     * @return true si ajouté avec succès, false si limite atteinte
     */
    public boolean addInteriorMode(String name, List<Integer> colors) {
        if (interiorModes.size() >= MAX_INTERIOR_MODES) {
            Log.w(TAG, "Limite de modes interior atteinte");
            return false;
        }
        
        int newNumber = interiorModes.size() + 1;
        LightModeData newMode = new LightModeData(newNumber, name, colors, true);
        interiorModes.add(newMode);
        saveInteriorModes(interiorModes);
        return true;
    }
    
    /**
     * Ajoute un nouveau mode exterior
     * @return true si ajouté avec succès, false si limite atteinte
     */
    public boolean addExteriorMode(String name, List<Integer> colors) {
        if (exteriorModes.size() >= MAX_EXTERIOR_MODES) {
            Log.w(TAG, "Limite de modes exterior atteinte");
            return false;
        }
        
        int newNumber = exteriorModes.size() + 1;
        LightModeData newMode = new LightModeData(newNumber, name, colors, true);
        exteriorModes.add(newMode);
        saveExteriorModes(exteriorModes);
        return true;
    }
    
    /**
     * Supprime un mode interior par index
     * @return true si supprimé avec succès, false si premier mode (Off)
     */
    public boolean removeInteriorMode(int index) {
        if (index < 0 || index >= interiorModes.size()) {
            Log.w(TAG, "Index invalide: " + index);
            return false;
        }
        
        // Seul le premier mode (Off, index 0) ne peut pas être supprimé
        if (index == 0) {
            Log.w(TAG, "Impossible de supprimer le mode Off");
            return false;
        }
        
        LightModeData mode = interiorModes.get(index);
        Log.d(TAG, "Suppression du mode: " + mode.getName() + " à l'index " + index);
        
        // Au lieu de supprimer, remplacer par un placeholder
        // Le numéro reste le même (index + 1)
        int modeNumber = index + 1;
        LightModeData placeholder = new LightModeData(
            modeNumber,
            "Click edit to add",
            Collections.singletonList(Color.GRAY),
            false  // Not editable (c'est un placeholder)
        );
        
        interiorModes.set(index, placeholder);
        saveInteriorModes(interiorModes);
        return true;
    }
    
    /**
     * Supprime un mode exterior par index
     * @return true si supprimé avec succès, false si premier mode (Off)
     */
    public boolean removeExteriorMode(int index) {
        if (index < 0 || index >= exteriorModes.size()) {
            Log.w(TAG, "Index invalide: " + index);
            return false;
        }
        
        // Seul le premier mode (Off, index 0) ne peut pas être supprimé
        if (index == 0) {
            Log.w(TAG, "Impossible de supprimer le mode Off");
            return false;
        }
        
        LightModeData mode = exteriorModes.get(index);
        Log.d(TAG, "Suppression du mode: " + mode.getName() + " à l'index " + index);
        
        // Au lieu de supprimer, remplacer par un placeholder
        // Le numéro reste le même (index + 1)
        int modeNumber = index + 1;
        LightModeData placeholder = new LightModeData(
            modeNumber,
            "Click edit to add",
            Collections.singletonList(Color.GRAY),
            false  // Not editable (c'est un placeholder)
        );
        
        exteriorModes.set(index, placeholder);
        saveExteriorModes(exteriorModes);
        return true;
    }
    
    /**
     * Réorganise les numéros de modes après une suppression
     */
    private void reorganizeModes(List<LightModeData> modes) {
        for (int i = 0; i < modes.size(); i++) {
            modes.get(i).setNumber(i + 1);
        }
    }
    
    /**
     * Met à jour un mode interior existant
     */
    public boolean updateInteriorMode(int index, String name, List<Integer> colors) {
        if (index < 0 || index >= interiorModes.size()) {
            return false;
        }
        
        LightModeData mode = interiorModes.get(index);
        if (!mode.isEditable()) {
            return false;
        }
        
        mode.setName(name);
        mode.setColors(colors);
        saveInteriorModes(interiorModes);
        return true;
    }
    
    /**
     * Met à jour un mode exterior existant
     */
    public boolean updateExteriorMode(int index, String name, List<Integer> colors) {
        if (index < 0 || index >= exteriorModes.size()) {
            return false;
        }
        
        LightModeData mode = exteriorModes.get(index);
        if (!mode.isEditable()) {
            return false;
        }
        
        mode.setName(name);
        mode.setColors(colors);
        saveExteriorModes(exteriorModes);
        return true;
    }
    
    /**
     * Remplace un placeholder par un vrai mode
     */
    public boolean replacePlaceholder(boolean isInterior, int index, String name, List<Integer> colors) {
        List<LightModeData> modes = isInterior ? interiorModes : exteriorModes;
        
        if (index < 0 || index >= modes.size()) {
            return false;
        }
        
        LightModeData mode = modes.get(index);
        if (!mode.isPlaceholder()) {
            return false;
        }
        
        mode.setName(name);
        mode.setColors(colors);
        mode.setEditable(true);
        
        if (isInterior) {
            saveInteriorModes(interiorModes);
        } else {
            saveExteriorModes(exteriorModes);
        }
        
        return true;
    }
}
