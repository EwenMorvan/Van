package com.van.management.ui.tabs;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.os.Handler;
import android.os.Looper;
import android.renderscript.RenderScript;
import android.util.Log;
import android.view.View;
import android.widget.TextView;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.constraintlayout.widget.ConstraintLayout;

import com.google.gson.Gson;
import com.van.management.R;
import com.van.management.data.HeaterCommand;
import com.van.management.data.LedCommand;
import com.van.management.data.LedData;
import com.van.management.data.LedStaticCommand;
import com.van.management.data.VanCommand;
import com.van.management.data.VanCommandExample;
import com.van.management.data.VanState;
import com.van.management.data.LightModeData;
import com.van.management.data.LightModesRepository;
import com.van.management.data.LedModeConfig;
import com.van.management.ui.activities.LightEditorActivity;
import com.van.management.ui.tabs.lightEditor.InteriorLightsEditorManager;
import com.van.management.utils.CircleEffectApplier;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Gère l'onglet Light avec les modes d'éclairage et brightness sliders
 */
public class LightTab extends BaseTab {
    private static final String TAG = "LightTab";
    
    private final Context context;
    private final RenderScript renderScript;
    private LightModesRepository repository;
    private ActivityResultLauncher<Intent> lightEditorLauncher;
    
    // Light Modes Selection
    private View selectedInteriorModeButton = null;
    private View selectedExteriorModeButton = null;
    private int selectedInteriorMode = 1; // Par défaut: Off
    private int selectedExteriorMode = 1; // Par défaut: Off

    private TextView editInteriorLightText;
    private TextView editExteriorLightText;
    private Dialog interiorLightsPopupDialog;
    private Dialog exteriorLightsPopupDialog;
    private InteriorLightsEditorManager interiorLightsEditorManager;
    private InteriorLightsEditorManager exteriorLightsEditorManager;
    
    // Variables pour la popup
    private View popupBackground;
    private View selectedButtonInPopup = null;
    
    // Brightness Sliders
    private View interiorBrightnessGaugeBg = null;
    private View interiorBrightnessGauge = null;
    private TextView interiorBrightnessText = null;
    private float interiorBrightnessPercent = 0.58f; // 58% par défaut
    
    private View exteriorBrightnessGaugeBg = null;
    private View exteriorBrightnessGauge = null;
    private TextView exteriorBrightnessText = null;
    private float exteriorBrightnessPercent = 0.58f; // 58% par défaut
    
    public LightTab(Context context, RenderScript renderScript) {
        this.context = context;
        this.renderScript = renderScript;
        this.repository = new LightModesRepository(context);
        
        // Initialize ActivityResultLauncher for LightEditorActivity
        if (context instanceof Activity) {
            Activity activity = (Activity) context;
            if (activity instanceof androidx.activity.ComponentActivity) {
                lightEditorLauncher = ((androidx.activity.ComponentActivity) activity)
                    .registerForActivityResult(
                        new ActivityResultContracts.StartActivityForResult(),
                        result -> {
                            if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                                handleLightEditorResult(result.getData());
                            }
                        }
                    );
            }
        }
    }
    
    /**
     * Ajoute automatiquement le préfixe "|" ou "||" au nom selon le nombre de strips configurés
     * @param originalName Le nom original (peut déjà contenir un préfixe)
     * @param ledConfig La configuration LED
     * @return Le nom avec le préfixe approprié
     */
    private String addStripPrefixToName(String originalName, LedModeConfig ledConfig) {
        // Remove any existing prefix first
        String cleanName = originalName.replaceFirst("^\\|+ ", "");
        
        // Vérifier si le 2ème strip existe ET n'est pas vide
        boolean copyStrip1 = ledConfig.isCopyStrip1();
        boolean hasStrip2Data = ledConfig.getStrip2Data() != null;
        boolean strip2IsEmpty = hasStrip2Data && ledConfig.getStrip2Data().isEmpty();
        boolean hasStrip2 = hasStrip2Data && !strip2IsEmpty;
        
        // Debug: log the ledConfig state
        Log.d(TAG, "addStripPrefixToName: copyStrip1=" + copyStrip1 + 
                   ", hasStrip2Data=" + hasStrip2Data + 
                   ", strip2IsEmpty=" + strip2IsEmpty + 
                   ", hasStrip2=" + hasStrip2);
        
        // Determine the appropriate prefix based on strip configuration
        String prefix;
        if (!copyStrip1 && !hasStrip2) {
            // Single strip mode (pas de strip2 ou strip2 vide)
            prefix = "| ";
            Log.d(TAG, "Using single strip prefix: |");
        } else {
            // Two strips mode (soit copyStrip1 = true, soit strip2 configuré)
            prefix = "|| ";
            Log.d(TAG, "Using two strips prefix: ||");
        }
        
        return prefix + cleanName;
    }
    
    private void handleLightEditorResult(Intent data) {
        try {
            // Get the saved LED configuration from the Intent
            String ledConfigJson = data.getStringExtra("LED_CONFIG");
            if (ledConfigJson == null) {
                Log.w(TAG, "No LED_CONFIG found in result intent");
                return;
            }
            
            // Deserialize the LedModeConfig
            Gson gson = new Gson();
            LedModeConfig ledConfig = gson.fromJson(ledConfigJson, LedModeConfig.class);
            
            // Get mode information from Intent
            String newName = data.getStringExtra(LightEditorActivity.EXTRA_MODE_NAME);
            boolean isInterior = data.getBooleanExtra(LightEditorActivity.EXTRA_IS_INTERIOR, true);
            int oldModeIndex = data.getIntExtra(LightEditorActivity.EXTRA_MODE_INDEX, -1);
            
            // Le numéro du mode = position dans la liste + 1
            int assignedNumber = oldModeIndex + 1;
            
            Log.d(TAG, "Received LED config: name=" + newName + ", assignedNumber=" + assignedNumber + 
                  ", interior=" + isInterior + ", index=" + oldModeIndex);
            
            // Close the popup if it's open
            if (isInterior && interiorLightsPopupDialog != null && interiorLightsPopupDialog.isShowing()) {
                interiorLightsPopupDialog.dismiss();
                interiorLightsPopupDialog = null;
            } else if (!isInterior && exteriorLightsPopupDialog != null && exteriorLightsPopupDialog.isShowing()) {
                exteriorLightsPopupDialog.dismiss();
                exteriorLightsPopupDialog = null;
            }
            
            // Remove popup background if present
            if (popupBackground != null && popupBackground.getParent() instanceof android.view.ViewGroup) {
                ((android.view.ViewGroup) popupBackground.getParent()).removeView(popupBackground);
                popupBackground = null;
            }
            
            // Add strip count prefix to name
            String prefixedName = addStripPrefixToName(newName, ledConfig);
            
            // Update the mode in the repository
            if (isInterior) {
                // Create or update the mode (numéro = index + 1)
                LightModeData updatedMode = new LightModeData(assignedNumber, prefixedName, true, ledConfig);
                
                // Update in repository at the index where it was
                repository.updateInteriorMode(oldModeIndex, updatedMode);
                
                // Refresh the entire UI to show the changes
                refreshInteriorModesUI();
                refreshMainUI(); // Also refresh main card
                
                Log.d(TAG, "Updated interior mode at index " + oldModeIndex + " with name: " + prefixedName + " (number: " + assignedNumber + ")");
            } else {
                // Create or update the mode (numéro = index + 1)
                LightModeData updatedMode = new LightModeData(assignedNumber, prefixedName, true, ledConfig);
                
                // Update in repository at the index where it was
                repository.updateExteriorMode(oldModeIndex, updatedMode);
                
                // Refresh the entire UI
                refreshExteriorModesUI();
                refreshMainUI(); // Also refresh main card
                
                Log.d(TAG, "Updated exterior mode at index " + oldModeIndex + " with name: " + prefixedName + " (number: " + assignedNumber + ")");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error handling light editor result", e);
        }
    }
    
    /**
     * Rafraîchit l'affichage des modes interior dans le LightTab
     */
    private void refreshInteriorModesUI() {
        List<LightModeData> visibleModes = repository.getVisibleInteriorModes();
        int[] buttonIds = {
            R.id.lightMode1Button, R.id.lightMode2Button, R.id.lightMode3Button,
            R.id.lightMode4Button, R.id.lightMode5Button, R.id.lightMode6Button
        };
        
        for (int i = 0; i < buttonIds.length && i < visibleModes.size(); i++) {
            LightModeData mode = visibleModes.get(i);
            updateLightModeInCard(R.id.interior_lights_mode_layout, buttonIds[i],
                mode.getNumber() + ".", mode.getName(), 
                mode.getColors(), mode.isPlaceholder());
        }
    }
    
    /**
     * Rafraîchit l'affichage des modes exterior dans le LightTab
     */
    private void refreshExteriorModesUI() {
        List<LightModeData> visibleModes = repository.getVisibleExteriorModes();
        int[] buttonIds = {
            R.id.lightMode1Button, R.id.lightMode2Button,
            R.id.lightMode3Button, R.id.lightMode4Button
        };
        
        for (int i = 0; i < buttonIds.length && i < visibleModes.size(); i++) {
            LightModeData mode = visibleModes.get(i);
            updateLightModeInCard(R.id.exterior_lights_mode_layout, buttonIds[i],
                mode.getNumber() + ".", mode.getName(), 
                mode.getColors(), mode.isPlaceholder());
        }
    }
    
    @Override
    protected void onInitialize() {
        Log.d(TAG, "Initialisation du LightTab");
        
        // Charger les modes depuis le repository
        List<LightModeData> interiorModes = repository.getVisibleInteriorModes();
        List<LightModeData> exteriorModes = repository.getVisibleExteriorModes();
        
        // Afficher les modes Interior (6 visibles)
        int[] interiorButtonIds = {
            R.id.lightMode1Button, R.id.lightMode2Button, R.id.lightMode3Button,
            R.id.lightMode4Button, R.id.lightMode5Button, R.id.lightMode6Button
        };
        
        for (int i = 0; i < interiorButtonIds.length && i < interiorModes.size(); i++) {
            LightModeData mode = interiorModes.get(i);
            updateLightModeInCard(R.id.interior_lights_mode_layout, interiorButtonIds[i], 
                mode.getNumber() + ".", mode.getName(), mode.getColors(), mode.isPlaceholder());
        }
        
        // Afficher les modes Exterior (4 visibles)
        int[] exteriorButtonIds = {
            R.id.lightMode1Button, R.id.lightMode2Button, 
            R.id.lightMode3Button, R.id.lightMode4Button
        };
        
        for (int i = 0; i < exteriorButtonIds.length && i < exteriorModes.size(); i++) {
            LightModeData mode = exteriorModes.get(i);
            updateLightModeInCard(R.id.exterior_lights_mode_layout, exteriorButtonIds[i], 
                mode.getNumber() + ".", mode.getName(), mode.getColors(), mode.isPlaceholder());
        }
        
        // Setup brightness sliders après un court délai pour que les vues soient complètement rendues
        new Handler(Looper.getMainLooper()).postDelayed(this::setupBrightnessSliders, 100);
        
        editInteriorLightText = rootView.findViewById(R.id.edit_interior_light_text);
        editExteriorLightText = rootView.findViewById(R.id.edit_exterior_light_text);

        // Configurer les listeners sur les boutons edit
        setupEditButtonListeners();
    }
    
    @Override
    public void updateUI(VanState vanState) {
        if (vanState == null || vanState.leds == null || vanState.leds.leds_roof1 == null) {
            return;
        }
        
        // Sélectionner automatiquement le mode qui correspond à current_mode
        int currentMode = vanState.leds.leds_roof1.current_mode + 1; // Sur l'esp32 les mode commencent a 0
        
        // Trouver et sélectionner le bouton correspondant dans Interior Lights
        View cardLayout = rootView.findViewById(R.id.interior_lights_mode_layout);
        if (cardLayout != null) {
            // Mapper le mode au bouton correspondant
            int buttonId = getButtonIdForMode(currentMode);
            if (buttonId != 0) {
                View includeLayout = cardLayout.findViewById(buttonId);
                if (includeLayout != null) {
                    View buttonContainer = includeLayout.findViewById(R.id.light_mode_button_container);
                    if (buttonContainer != null) {
                        onLightModeSelected(true, currentMode, buttonContainer, false);
                    }
                }
            }
        }
    }
    
    private int getButtonIdForMode(int mode) {
        switch (mode) {
            case 1: return R.id.lightMode1Button;
            case 2: return R.id.lightMode2Button;
            case 3: return R.id.lightMode3Button;
            case 4: return R.id.lightMode4Button;
            case 5: return R.id.lightMode5Button;
            case 6: return R.id.lightMode6Button;
            default: return 0;
        }
    }
    
    private void updateLightModeInCard(int cardLayoutId, int includeLayoutId, String number, String modeName, List<Integer> colors, boolean isPlaceholder) {
        if (rootView == null) return;
        
        // Récupérer la carte (interior ou exterior)
        View cardLayout = rootView.findViewById(cardLayoutId);
        
        if (cardLayout == null) {
            Log.w(TAG, "updateLightModeInCard: Carte non trouvée");
            return;
        }
        
        // Récupérer le layout include à l'intérieur de la carte
        View includeLayout = cardLayout.findViewById(includeLayoutId);
        
        if (includeLayout == null) {
            Log.w(TAG, "updateLightModeInCard: Include non trouvé pour " + modeName);
            return;
        }
        
        // Récupérer les vues à l'intérieur de l'include
        TextView numberText = includeLayout.findViewById(R.id.light_mode_number_text);
        TextView nameText = includeLayout.findViewById(R.id.light_mode_name_text);
        View circleView = includeLayout.findViewById(R.id.circleView);
        View buttonContainer = includeLayout.findViewById(R.id.light_mode_button_container);
        
        if (numberText == null || nameText == null || circleView == null || buttonContainer == null) {
            Log.w(TAG, "updateLightModeInCard: Vues internes non trouvées pour " + modeName);
            return;
        }
        
        // Mettre à jour le numéro et le nom
        numberText.setText(number);
        nameText.setText(modeName);
        
        // Mettre à jour le cercle avec les couleurs
        circleView.setBackground(new CircleEffectApplier(renderScript, colors));
        
        // Ajouter le click listener pour sélectionner ce mode
        final boolean isInterior = (cardLayoutId == R.id.interior_lights_mode_layout);
        final int modeNumber = Integer.parseInt(number.replace(".", ""));
        
        if (isPlaceholder) {
            // Pour les placeholders: désactiver le bouton et ne pas ajouter de listener
            buttonContainer.setEnabled(false);
            buttonContainer.setAlpha(0.5f);
            buttonContainer.setOnClickListener(null);
        } else {
            // Pour les modes normaux: activer et ajouter le listener
            buttonContainer.setEnabled(true);
            buttonContainer.setAlpha(1.0f);
            buttonContainer.setOnClickListener(v -> {
                onLightModeSelected(isInterior, modeNumber, buttonContainer, true);
            });
            
            // Sélectionner par défaut le mode 1 (Off) au premier chargement
            if (modeNumber == 1) {
                buttonContainer.setSelected(true);
                if (isInterior) {
                    selectedInteriorModeButton = buttonContainer;
                } else {
                    selectedExteriorModeButton = buttonContainer;
                }
            }
        }
    }
    
    private void onLightModeSelected(boolean isInterior, int modeNumber, View buttonContainer, boolean fromUser) {
        Log.d(TAG, "Mode sélectionné: " + (isInterior ? "Interior" : "Exterior") + " #" + modeNumber + 
            " (fromUser: " + fromUser + ")");
        
        if (isInterior) {
            // Désélectionner l'ancien bouton
            if (selectedInteriorModeButton != null) {
                selectedInteriorModeButton.setSelected(false);
            }
            // Sélectionner le nouveau
            buttonContainer.setSelected(true);
            selectedInteriorModeButton = buttonContainer;
            selectedInteriorMode = modeNumber;
            
            // Envoyer la commande BLE SEULEMENT si l'action vient de l'utilisateur
            if (fromUser && commandSender != null) {
                VanCommand command = null;
                
                // Try to get the saved LED configuration for this mode
                List<LightModeData> modes = repository.getVisibleInteriorModes();
                int modeIndex = modeNumber - 1; // Mode numbers start at 1, index at 0
                
                if (modeIndex >= 0 && modeIndex < modes.size()) {
                    LightModeData mode = modes.get(modeIndex);
                    command = mode.createVanCommand();
                    
                    if (command != null) {
                        Log.d(TAG, "Using saved LED configuration for interior mode " + modeNumber);
                    }
                }
                
                // Fallback to hardcoded commands if no saved configuration
                if (command == null) {
                    VanCommandExample vanCommandExample = new VanCommandExample();
                    switch (modeNumber) {
                        case 1:
                            command = vanCommandExample.createRoofOffCommand();
                            break;
                        case 2:
                            command = vanCommandExample.createRoofWhiteCommand();
                            break;
                        case 3:
                            command = vanCommandExample.createRoof1LED1StripCommand();
                            break;
                        case 4:
                            command = vanCommandExample.createRoofBlueCommand();
                            break;
                        case 5:
                            command = vanCommandExample.createRoofRainbowDynamicCommand();
                            break;
                        case 6:
                            command = vanCommandExample.createRoofRainbowStaticCommand();
                            break;
                    }
                    Log.d(TAG, "Using hardcoded command for interior mode " + modeNumber);
                }

                if (command != null) {
                    commandSender.sendBinaryCommand(command);
                }
            }
        } else {
            // Désélectionner l'ancien bouton
            if (selectedExteriorModeButton != null) {
                selectedExteriorModeButton.setSelected(false);
            }
            // Sélectionner le nouveau
            buttonContainer.setSelected(true);
            selectedExteriorModeButton = buttonContainer;
            selectedExteriorMode = modeNumber;
            
            // Envoyer la commande BLE SEULEMENT si l'action vient de l'utilisateur
            if (fromUser && commandSender != null) {
                VanCommand command = null;
                
                // Try to get the saved LED configuration for this mode
                List<LightModeData> modes = repository.getVisibleExteriorModes();
                int modeIndex = modeNumber - 1; // Mode numbers start at 1, index at 0
                
                if (modeIndex >= 0 && modeIndex < modes.size()) {
                    LightModeData mode = modes.get(modeIndex);
                    command = mode.createVanCommand();
                    
                    if (command != null) {
                        Log.d(TAG, "Using saved LED configuration for exterior mode " + modeNumber);
                    }
                }
                
                // For exterior modes, we might add fallback commands later if needed
                if (command == null) {
                    Log.w(TAG, "No command available for exterior mode " + modeNumber);
                }
                
                if (command != null) {
                    commandSender.sendBinaryCommand(command);
                }
            }
        }
    }
    
    private void setupBrightnessSliders() {
        if (rootView == null) return;
        
        // Interior - chercher directement dans la vue chargée
        View interiorCard = rootView.findViewById(R.id.interior_light_brightness_content_layout);
        if (interiorCard != null) {
            interiorBrightnessGaugeBg = interiorCard.findViewById(R.id.interior_light_brightness_gauge_bg);
            interiorBrightnessGauge = interiorCard.findViewById(R.id.interior_light_brightness_gauge);
            interiorBrightnessText = interiorCard.findViewById(R.id.interior_light_brightness_text);
            
            if (interiorBrightnessGaugeBg != null && interiorBrightnessGauge != null && interiorBrightnessText != null) {
                setupBrightnessSlider(interiorBrightnessGaugeBg, interiorBrightnessGauge, 
                    interiorBrightnessText, true);
                Log.d(TAG, "Interior brightness slider configuré");
            } else {
                Log.w(TAG, "Vues interior brightness non trouvées");
            }
        }
        
        // Exterior - chercher directement dans la vue chargée
        View exteriorCard = rootView.findViewById(R.id.exterior_light_brightness_content_layout);
        if (exteriorCard != null) {
            exteriorBrightnessGaugeBg = exteriorCard.findViewById(R.id.exterior_light_brightness_gauge_bg);
            exteriorBrightnessGauge = exteriorCard.findViewById(R.id.exterior_light_brightness_gauge);
            exteriorBrightnessText = exteriorCard.findViewById(R.id.exterior_light_brightness_text);
            
            if (exteriorBrightnessGaugeBg != null && exteriorBrightnessGauge != null && exteriorBrightnessText != null) {
                setupBrightnessSlider(exteriorBrightnessGaugeBg, exteriorBrightnessGauge, 
                    exteriorBrightnessText, false);
                Log.d(TAG, "Exterior brightness slider configuré");
            } else {
                Log.w(TAG, "Vues exterior brightness non trouvées");
            }
        }
    }
    
    @SuppressLint("ClickableViewAccessibility")
    private void setupBrightnessSlider(View backgroundView, View gaugeView, TextView textView, boolean isInterior) {
        Log.d(TAG, "Setup brightness slider pour " + (isInterior ? "interior" : "exterior"));
        
        // S'assurer que la vue est clickable
        backgroundView.setClickable(true);
        
        backgroundView.setOnTouchListener((v, event) -> {
            float x = event.getX();
            float width = v.getWidth();
            // Bloquer le minimum à 10%
            float percent = Math.max(0.1f, Math.min(1f, x / width));
            
            Log.d(TAG, "Touch event: " + event.getAction() + " x=" + x + " width=" + width + " percent=" + percent);
            
            if (event.getAction() == android.view.MotionEvent.ACTION_DOWN || 
                event.getAction() == android.view.MotionEvent.ACTION_MOVE) {
                
                // Mettre à jour la gauge
                ConstraintLayout.LayoutParams params = 
                    (ConstraintLayout.LayoutParams) gaugeView.getLayoutParams();
                params.matchConstraintPercentWidth = percent;
                gaugeView.setLayoutParams(params);
                
                // Mettre à jour le texte
                int percentValue = Math.round(percent * 100);
                textView.setText(percentValue + "%");
                
                // Sauvegarder la valeur
                if (isInterior) {
                    interiorBrightnessPercent = percent;
                } else {
                    exteriorBrightnessPercent = percent;
                }
                
                return true;
            } else if (event.getAction() == android.view.MotionEvent.ACTION_UP) {
                int percentValue = Math.round(percent * 100);
                Log.d(TAG, "Brightness " + (isInterior ? "interior" : "exterior") + 
                    " changée à " + percentValue + "%");
                
                // Envoyer la commande BLE avec le mode actuel et la nouvelle luminosité
                if (commandSender != null) {
                    int target = isInterior ? 0 : 1;
                    int currentMode = isInterior ? (selectedInteriorMode - 1) : (selectedExteriorMode - 1);
                    int brightness = Math.round(percent * 255);
                    
//                    com.van.management.data.VanCommand command =
//                        com.van.management.data.VanCommand.createLedCommand(target, currentMode, brightness);
//
//                    Log.d(TAG, "Envoi commande brightness: target=" + target + ", mode=" + currentMode + ", brightness=" + brightness);
//                    commandSender.sendCommand(command);
                }
                
                return true;
            }
            
            return false;
        });
    }
    
    // Getters pour les valeurs sélectionnées (utile pour envoyer les commandes BLE)
    public int getSelectedInteriorMode() {
        return selectedInteriorMode;
    }
    
    public int getSelectedExteriorMode() {
        return selectedExteriorMode;
    }
    
    public float getInteriorBrightness() {
        return interiorBrightnessPercent;
    }
    
    public float getExteriorBrightness() {
        return exteriorBrightnessPercent;
    }
    
    /**
     * Rafraîchit l'UI principale avec les modes mis à jour depuis le repository
     */
    private void refreshMainUI() {
        // Charger les modes depuis le repository
        List<LightModeData> interiorModes = repository.getVisibleInteriorModes();
        List<LightModeData> exteriorModes = repository.getVisibleExteriorModes();
        
        // Afficher les modes Interior (6 visibles)
        int[] interiorButtonIds = {
            R.id.lightMode1Button, R.id.lightMode2Button, R.id.lightMode3Button,
            R.id.lightMode4Button, R.id.lightMode5Button, R.id.lightMode6Button
        };
        
        for (int i = 0; i < interiorButtonIds.length && i < interiorModes.size(); i++) {
            LightModeData mode = interiorModes.get(i);
            updateLightModeInCard(R.id.interior_lights_mode_layout, interiorButtonIds[i], 
                mode.getNumber() + ".", mode.getName(), mode.getColors(), mode.isPlaceholder());
        }
        
        // Afficher les modes Exterior (4 visibles)
        int[] exteriorButtonIds = {
            R.id.lightMode1Button, R.id.lightMode2Button, 
            R.id.lightMode3Button, R.id.lightMode4Button
        };
        
        for (int i = 0; i < exteriorButtonIds.length && i < exteriorModes.size(); i++) {
            LightModeData mode = exteriorModes.get(i);
            updateLightModeInCard(R.id.exterior_lights_mode_layout, exteriorButtonIds[i], 
                mode.getNumber() + ".", mode.getName(), mode.getColors(), mode.isPlaceholder());
        }
    }
    
    /**
     * Configure les listeners pour les boutons d'édition des modes Interior et Exterior
     */
    private void setupEditButtonListeners() {
        // Listener pour le bouton edit Interior Lights
        if (editInteriorLightText != null) {
            View interiorEditButton = rootView.findViewById(R.id.interior_light_edit_button);
            if (interiorEditButton != null) {
                interiorEditButton.setOnClickListener(v -> {
                    showLightsEditorPopup(true);
                });
            }
        }
        
        // Listener pour le bouton edit Exterior Lights
        if (editExteriorLightText != null) {
            View exteriorEditButton = rootView.findViewById(R.id.exterior_light_edit_button);
            if (exteriorEditButton != null) {
                exteriorEditButton.setOnClickListener(v -> {
                    showLightsEditorPopup(false);
                });
            }
        }
    }
    
    /**
     * Affiche la popup d'édition des modes de lumière
     * @param isInterior true pour Interior Lights, false pour Exterior Lights
     */
    private void showLightsEditorPopup(boolean isInterior) {
        Log.d(TAG, "Ouverture popup " + (isInterior ? "Interior" : "Exterior") + " Lights Editor");
        
        // Créer un fond noir semi-transparent pour cacher toute l'interface (y compris les tabs)
        popupBackground = new View(context);
        popupBackground.setLayoutParams(new android.widget.FrameLayout.LayoutParams(
            android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
            android.widget.FrameLayout.LayoutParams.MATCH_PARENT
        ));
        popupBackground.setBackgroundColor(android.graphics.Color.argb(220, 0, 0, 0)); // Noir à 86% d'opacité
        
        // Ajouter le fond au niveau de la racine de l'activité pour couvrir aussi les tabs
        android.view.ViewGroup rootActivity = (android.view.ViewGroup) ((android.app.Activity) context).getWindow().getDecorView().findViewById(android.R.id.content);
        if (rootActivity != null) {
            rootActivity.addView(popupBackground);
        }
        
        // Créer le dialog avec style transparent
        Dialog dialog = new Dialog(context, android.R.style.Theme_Black_NoTitleBar_Fullscreen);
        dialog.setContentView(R.layout.interior_light_edit_card);
        
        // Rendre le fond du dialog transparent
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawableResource(android.R.color.transparent);
            dialog.getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        }
        
        // Configurer le titre
        TextView titleView = dialog.findViewById(R.id.title);
        if (titleView != null) {
            titleView.setText(isInterior ? "Interior Lights Editor" : "Exterior Lights Editor");
        }
        
        // Configurer le bouton de fermeture
        View closeButton = dialog.findViewById(R.id.close_button);
        if (closeButton != null) {
            closeButton.setOnClickListener(v -> {
                // Supprimer le fond noir immédiatement avant de fermer le dialog
                if (popupBackground != null && popupBackground.getParent() instanceof android.view.ViewGroup) {
                    ((android.view.ViewGroup) popupBackground.getParent()).removeView(popupBackground);
                    popupBackground = null;
                }
                dialog.dismiss();
            });
        }
        
        // Calculer la hauteur pour 6 boutons visibles (interior) ou 5 (exterior) + marges
        int buttonHeight = context.getResources().getDimensionPixelSize(R.dimen.card_button_large);
        int marginBetween = context.getResources().getDimensionPixelSize(R.dimen.margin_xlarge);
        int paddingVertical = context.getResources().getDimensionPixelSize(R.dimen.margin_medium);
        
        // Pour interior: 6 boutons (3 par colonne), pour exterior: 5 boutons (3 sur une colonne, 2 sur l'autre)
        int visibleButtonsPerColumn = isInterior ? 3 : 3; // 3 boutons visibles par colonne
        int scrollViewHeight = (visibleButtonsPerColumn * buttonHeight) + ((visibleButtonsPerColumn - 1) * marginBetween) + (2 * paddingVertical);
        
        // Appliquer la hauteur à la ScrollView
        android.widget.ScrollView scrollView = dialog.findViewById(R.id.light_modes_scrollview);
        if (scrollView != null) {
            android.view.ViewGroup.LayoutParams params = scrollView.getLayoutParams();
            params.height = scrollViewHeight;
            scrollView.setLayoutParams(params);
        }
        
        // Créer le manager pour gérer les boutons dynamiques
        InteriorLightsEditorManager editorManager = new InteriorLightsEditorManager(context, dialog.findViewById(R.id.interior_lights_mode_layout), renderScript);
        
        // Charger TOUS les modes (incluant placeholders) - jusqu'à 20 pour gestion complète
        List<LightModeData> allModesWithPlaceholders = isInterior ? repository.getAllInteriorModesWithPlaceholders() : repository.getAllExteriorModesWithPlaceholders();
        
        // Charger seulement les modes NON-placeholders pour l'affichage dans le popup
        List<LightModeData> allModes = new ArrayList<>();
        for (LightModeData mode : allModesWithPlaceholders) {
            if (!mode.isPlaceholder()) {
                allModes.add(mode);
            }
        }
        
        int maxModes = isInterior ? LightModesRepository.MAX_INTERIOR_MODES : LightModesRepository.MAX_EXTERIOR_MODES;
        
        // Ajouter les modes existants dans l'éditeur (sans placeholders)
        for (LightModeData mode : allModes) {
            editorManager.addLightModeButton(mode.getName(), mode.getColors());
        }
        
        // Réinitialiser la sélection
        selectedButtonInPopup = null;
        
        // Configurer le listener de swap pour sauvegarder les changements
        editorManager.setOnButtonSwapListener((fromIndex, toIndex) -> {
            Log.d(TAG, "Swapping modes: " + fromIndex + " <-> " + toIndex);
            
            // Trouver les index réels dans allModesWithPlaceholders
            // Les index dans editorManager correspondent aux modes NON-placeholders affichés
            List<Integer> realIndices = new ArrayList<>();
            for (int i = 0; i < allModesWithPlaceholders.size(); i++) {
                if (!allModesWithPlaceholders.get(i).isPlaceholder()) {
                    realIndices.add(i);
                }
            }
            
            if (fromIndex < realIndices.size() && toIndex < realIndices.size()) {
                int realFromIndex = realIndices.get(fromIndex);
                int realToIndex = realIndices.get(toIndex);
                
                Log.d(TAG, "Real indices: " + realFromIndex + " <-> " + realToIndex);
                
                // Swapper dans le repository
                if (isInterior) {
                    repository.swapInteriorModes(realFromIndex, realToIndex);
                } else {
                    repository.swapExteriorModes(realFromIndex, realToIndex);
                }
                
                // Rafraîchir l'UI principale
                if (isInterior) {
                    refreshInteriorModesUI();
                } else {
                    refreshExteriorModesUI();
                }
                
                // Recharger la popup avec les nouvelles positions
                allModesWithPlaceholders.clear();
                allModesWithPlaceholders.addAll(isInterior ? 
                    repository.getAllInteriorModesWithPlaceholders() : 
                    repository.getAllExteriorModesWithPlaceholders());
            }
        });
        
        // Configurer les boutons Add New et Delete
        View addNewButton = dialog.findViewById(R.id.interior_light_add_new_button);
        TextView addNewText = dialog.findViewById(R.id.add_new_interior_light_text);
        
        // Créer une référence finale pour deleteButton pour l'utiliser dans le callback
        View[] deleteButtonRef = new View[1];
        deleteButtonRef[0] = dialog.findViewById(R.id.interior_light_delete_button);
        View deleteButton = deleteButtonRef[0];
        
        // Bouton Edit
        View editButton = dialog.findViewById(R.id.interior_light_edit_mode_button);
        
        // Bouton Apply
        View applyButton = dialog.findViewById(R.id.interior_light_apply_mode_button);
        
        // Configurer le listener de clic pour gérer la sélection
        editorManager.setOnButtonClickListener((buttonView, buttonName) -> {
            // Désélectionner l'ancien bouton
            if (selectedButtonInPopup != null) {
                selectedButtonInPopup.setSelected(false);
            }
            
            // Sélectionner le nouveau
            buttonView.setSelected(true);
            selectedButtonInPopup = buttonView;
            
            Log.d(TAG, "Bouton sélectionné: " + buttonName);
            
            // Mettre à jour l'état des boutons Delete et Edit
            // Le premier bouton (Off) ne peut pas être supprimé ni édité
            int selectedIndex = -1;
            List<InteriorLightsEditorManager.LightMode> modes = editorManager.getLightModes();
            for (int i = 0; i < modes.size(); i++) {
                View container = modes.get(i).buttonView.findViewById(R.id.light_mode_button_container);
                if (container == buttonView) {
                    selectedIndex = i;
                    break;
                }
            }
            
            if (deleteButtonRef[0] != null) {
                if (selectedIndex == 0) {
                    // Premier bouton : ne peut pas être supprimé
                    deleteButtonRef[0].setEnabled(false);
                    deleteButtonRef[0].setAlpha(0.5f);
                } else {
                    deleteButtonRef[0].setEnabled(true);
                    deleteButtonRef[0].setAlpha(1.0f);
                }
            }
            
            if (editButton != null) {
                if (selectedIndex == 0) {
                    // Premier bouton (OFF) : ne peut pas être édité
                    editButton.setEnabled(false);
                    editButton.setAlpha(0.5f);
                } else {
                    editButton.setEnabled(true);
                    editButton.setAlpha(1.0f);
                }
            }
            
            // Bouton Apply - activer si un mode est sélectionné
            if (applyButton != null) {
                applyButton.setEnabled(true);
                applyButton.setAlpha(1.0f);
            }
        });
        
        // Désactiver les boutons Delete, Edit et Apply au début
        if (deleteButton != null) {
            deleteButton.setEnabled(false);
            deleteButton.setAlpha(0.5f);
        }
        
        if (editButton != null) {
            editButton.setEnabled(false);
            editButton.setAlpha(0.5f);
        }
        
        if (applyButton != null) {
            applyButton.setEnabled(false);
            applyButton.setAlpha(0.5f);
        }
        
        // Variable pour tracker si des changements ont été faits
        final boolean[] hasChanges = {false};
        
        // Fonction pour mettre à jour l'état des boutons
        Runnable updateButtonStates = () -> {
            int buttonCount = editorManager.getButtonCount();
            
            // Compter les slots disponibles (placeholders) dans la liste complète
            int availableSlots = 0;
            for (LightModeData mode : allModesWithPlaceholders) {
                if (mode.isPlaceholder()) {
                    availableSlots++;
                }
            }
            
            // Bouton Add New - désactiver seulement s'il n'y a AUCUN slot disponible
            if (addNewButton != null && addNewText != null) {
                if (availableSlots == 0) {
                    addNewButton.setEnabled(false);
                    addNewButton.setAlpha(0.5f);
                    addNewText.setText("Max " + maxModes + " modes");
                } else {
                    addNewButton.setEnabled(true);
                    addNewButton.setAlpha(1.0f);
                    addNewText.setText("➕ Add New");
                }
            }
            
            // Bouton Delete
            if (deleteButton != null) {
                if (selectedButtonInPopup != null) {
                    deleteButton.setEnabled(true);
                    deleteButton.setAlpha(1.0f);
                } else {
                    deleteButton.setEnabled(false);
                    deleteButton.setAlpha(0.5f);
                }
            }
        };
        
        if (addNewButton != null) {
            addNewButton.setOnClickListener(v -> {
                // Trouver le premier slot placeholder disponible dans la liste complète
                int firstPlaceholderIndex = -1;
                for (int i = 0; i < allModesWithPlaceholders.size(); i++) {
                    if (allModesWithPlaceholders.get(i).isPlaceholder()) {
                        firstPlaceholderIndex = i;
                        break;
                    }
                }
                
                if (firstPlaceholderIndex < 0) {
                    android.widget.Toast.makeText(context, "No more slots available", android.widget.Toast.LENGTH_SHORT).show();
                    return;
                }
                
                // Calculer le numéro du nouveau mode (index + 1)
                int newModeNumber = firstPlaceholderIndex + 1;
                String newModeName = "Mode " + newModeNumber;
                
                Log.d(TAG, "Creating new mode at index " + firstPlaceholderIndex + " (number " + newModeNumber + ")");
                
                // Fermer le popup
                dialog.dismiss();
                
                // Lancer directement l'éditeur pour le nouveau mode
                Intent intent = new Intent(context, LightEditorActivity.class);
                intent.putExtra(LightEditorActivity.EXTRA_MODE_NAME, newModeName);
                intent.putExtra(LightEditorActivity.EXTRA_MODE_NUMBER, newModeNumber);
                intent.putExtra(LightEditorActivity.EXTRA_IS_INTERIOR, isInterior);
                intent.putExtra(LightEditorActivity.EXTRA_MODE_INDEX, firstPlaceholderIndex);
                
                // Launch with ActivityResultLauncher
                if (lightEditorLauncher != null) {
                    lightEditorLauncher.launch(intent);
                } else {
                    ((Activity) context).startActivity(intent);
                }
            });
        }
        
        if (editButton != null) {
            editButton.setOnClickListener(v -> {
                if (selectedButtonInPopup != null) {
                    // Trouver le mode sélectionné dans le popup (liste filtrée)
                    int selectedIndexInPopup = -1;
                    String modeName = "";
                    List<InteriorLightsEditorManager.LightMode> modes = editorManager.getLightModes();
                    for (int i = 0; i < modes.size(); i++) {
                        View container = modes.get(i).buttonView.findViewById(R.id.light_mode_button_container);
                        if (container == selectedButtonInPopup) {
                            selectedIndexInPopup = i;
                            modeName = modes.get(i).name;
                            break;
                        }
                    }
                    
                    if (selectedIndexInPopup == 0) {
                        // OFF mode ne peut pas être édité
                        android.widget.Toast.makeText(context, "Cannot edit OFF mode", android.widget.Toast.LENGTH_SHORT).show();
                        return;
                    }
                    
                    // Trouver l'index RÉEL dans le repository (avec placeholders)
                    int realIndexInRepo = -1;
                    if (selectedIndexInPopup < allModes.size()) {
                        LightModeData selectedMode = allModes.get(selectedIndexInPopup);
                        // Chercher ce mode dans la liste complète
                        for (int i = 0; i < allModesWithPlaceholders.size(); i++) {
                            if (allModesWithPlaceholders.get(i).getName().equals(selectedMode.getName()) &&
                                allModesWithPlaceholders.get(i).getNumber() == selectedMode.getNumber()) {
                                realIndexInRepo = i;
                                break;
                            }
                        }
                    }
                    
                    if (realIndexInRepo < 0) {
                        android.widget.Toast.makeText(context, "Error: mode not found", android.widget.Toast.LENGTH_SHORT).show();
                        return;
                    }
                    
                    LightModeData selectedMode = allModesWithPlaceholders.get(realIndexInRepo);
                    
                    // Lancer l'activité LightEditor
                    Intent intent = new Intent(context, LightEditorActivity.class);
                    intent.putExtra(LightEditorActivity.EXTRA_MODE_NAME, selectedMode.getName());
                    intent.putExtra(LightEditorActivity.EXTRA_MODE_NUMBER, selectedMode.getNumber());
                    intent.putExtra(LightEditorActivity.EXTRA_IS_INTERIOR, isInterior);
                    intent.putExtra(LightEditorActivity.EXTRA_MODE_INDEX, realIndexInRepo);
                    
                    // Passer le LedModeConfig existant si disponible
                    if (selectedMode.getLedConfig() != null) {
                        String existingConfigJson = new Gson().toJson(selectedMode.getLedConfig());
                        intent.putExtra("EXISTING_LED_CONFIG", existingConfigJson);
                        Log.d(TAG, "Passing existing LED config for mode: " + modeName);
                    }
                    
                    // Launch with ActivityResultLauncher to receive LED configuration
                    if (lightEditorLauncher != null) {
                        lightEditorLauncher.launch(intent);
                    } else {
                        // Fallback for non-ComponentActivity contexts
                        ((Activity) context).startActivity(intent);
                    }
                    
                    Log.d(TAG, "Launching LightEditor for mode: " + modeName + " at real index: " + realIndexInRepo);
                }
            });
        }
        
        if (deleteButton != null) {
            deleteButton.setOnClickListener(v -> {
                if (selectedButtonInPopup != null && editorManager.getButtonCount() > 1) {
                    // Trouver le mode sélectionné dans le popup (liste filtrée)
                    int selectedIndexInPopup = -1;
                    String modeName = "";
                    List<InteriorLightsEditorManager.LightMode> modes = editorManager.getLightModes();
                    for (int i = 0; i < modes.size(); i++) {
                        View container = modes.get(i).buttonView.findViewById(R.id.light_mode_button_container);
                        if (container == selectedButtonInPopup) {
                            selectedIndexInPopup = i;
                            modeName = modes.get(i).name;
                            break;
                        }
                    }
                    
                    // Trouver l'index RÉEL dans le repository (avec placeholders)
                    int realIndexInRepo = -1;
                    if (selectedIndexInPopup < allModes.size()) {
                        LightModeData selectedMode = allModes.get(selectedIndexInPopup);
                        // Chercher ce mode dans la liste complète
                        for (int i = 0; i < allModesWithPlaceholders.size(); i++) {
                            if (allModesWithPlaceholders.get(i).getName().equals(selectedMode.getName()) &&
                                allModesWithPlaceholders.get(i).getNumber() == selectedMode.getNumber()) {
                                realIndexInRepo = i;
                                break;
                            }
                        }
                    }
                    
                    final int indexToDelete = realIndexInRepo;
                    final String modeNameFinal = modeName;
                    
                    Log.d(TAG, "Tentative de suppression: nom=" + modeName + ", indexPopup=" + selectedIndexInPopup + ", realIndex=" + realIndexInRepo);
                    
                    // Afficher un dialog de confirmation avec le numéro et le nom
                    String message = "Are you sure you want to delete " + (selectedIndexInPopup + 1) + ". " + modeName + "?";
                    new android.app.AlertDialog.Builder(context)
                        .setTitle("Delete Mode")
                        .setMessage(message)
                        .setPositiveButton("Yes", (dialogInterface, which) -> {
                            if (indexToDelete < 0) {
                                Log.e(TAG, "Index invalide pour la suppression: " + indexToDelete);
                                android.widget.Toast.makeText(context, "Error: mode not found", android.widget.Toast.LENGTH_SHORT).show();
                                return;
                            }
                            
                            // Supprimer le bouton sélectionné dans le manager
                            boolean deleted = editorManager.removeButton(selectedButtonInPopup);
                            if (deleted) {
                                // Supprimer aussi dans le repository avec le bon index
                                boolean repoDeleted = isInterior ? 
                                    repository.removeInteriorMode(indexToDelete) : 
                                    repository.removeExteriorMode(indexToDelete);
                                
                                if (repoDeleted) {
                                    selectedButtonInPopup = null;
                                    // Marquer qu'il y a eu des changements
                                    hasChanges[0] = true;
                                    Log.d(TAG, "Mode supprimé avec succès: " + modeNameFinal);
                                    updateButtonStates.run();
                                } else {
                                    Log.e(TAG, "Échec de suppression dans le repository pour: " + modeNameFinal);
                                    android.widget.Toast.makeText(context, "Cannot delete the first mode", android.widget.Toast.LENGTH_SHORT).show();
                                }
                            } else {
                                Log.e(TAG, "Échec de suppression dans le manager pour: " + modeNameFinal);
                                android.widget.Toast.makeText(context, "Cannot delete this mode", android.widget.Toast.LENGTH_SHORT).show();
                            }
                        })
                        .setNegativeButton("No", null)
                        .show();
                }
            });
        }
        
        // Bouton Apply - envoyer la commande BLE pour tester le mode sélectionné
        if (applyButton != null) {
            applyButton.setOnClickListener(v -> {
                if (selectedButtonInPopup != null) {
                    // Trouver le mode sélectionné dans le popup (liste filtrée)
                    int selectedIndexInPopup = -1;
                    String modeName = "";
                    List<InteriorLightsEditorManager.LightMode> modes = editorManager.getLightModes();
                    for (int i = 0; i < modes.size(); i++) {
                        View container = modes.get(i).buttonView.findViewById(R.id.light_mode_button_container);
                        if (container == selectedButtonInPopup) {
                            selectedIndexInPopup = i;
                            modeName = modes.get(i).name;
                            break;
                        }
                    }
                    
                    // Trouver l'index RÉEL dans le repository (avec placeholders)
                    int realIndexInRepo = -1;
                    if (selectedIndexInPopup < allModes.size()) {
                        LightModeData selectedMode = allModes.get(selectedIndexInPopup);
                        // Chercher ce mode dans la liste complète
                        for (int i = 0; i < allModesWithPlaceholders.size(); i++) {
                            if (allModesWithPlaceholders.get(i).getName().equals(selectedMode.getName()) &&
                                allModesWithPlaceholders.get(i).getNumber() == selectedMode.getNumber()) {
                                realIndexInRepo = i;
                                break;
                            }
                        }
                    }
                    
                    if (realIndexInRepo < 0) {
                        android.widget.Toast.makeText(context, "Error: mode not found", android.widget.Toast.LENGTH_SHORT).show();
                        return;
                    }
                    
                    LightModeData selectedMode = allModesWithPlaceholders.get(realIndexInRepo);
                    
                    // Créer et envoyer la commande BLE
                    VanCommand command = selectedMode.createVanCommand();
                    if (command != null) {
                        // Envoyer via BLE
                        if (commandSender != null) {
                            commandSender.sendBinaryCommand(command);
                            android.widget.Toast.makeText(context, "Applying " + modeName + "...", android.widget.Toast.LENGTH_SHORT).show();
                            Log.d(TAG, "Sending BLE command for mode: " + modeName);
                        } else {
                            android.widget.Toast.makeText(context, "BLE not connected", android.widget.Toast.LENGTH_SHORT).show();
                            Log.w(TAG, "BLE Manager is null, cannot send command");
                        }
                    } else {
                        android.widget.Toast.makeText(context, "No LED configuration for this mode", android.widget.Toast.LENGTH_SHORT).show();
                        Log.w(TAG, "No LED config available for mode: " + modeName);
                    }
                }
            });
        }
        
        // Mise à jour initiale de l'état des boutons
        updateButtonStates.run();
        
        // Stocker la référence du dialog et du manager
        if (isInterior) {
            interiorLightsPopupDialog = dialog;
            interiorLightsEditorManager = editorManager;
        } else {
            exteriorLightsPopupDialog = dialog;
            exteriorLightsEditorManager = editorManager;
        }
        
        // Gérer la fermeture du dialog
        dialog.setOnDismissListener(d -> {
            // Supprimer le fond noir immédiatement
            if (popupBackground != null && popupBackground.getParent() instanceof android.view.ViewGroup) {
                ((android.view.ViewGroup) popupBackground.getParent()).removeView(popupBackground);
                popupBackground = null;
            }
            selectedButtonInPopup = null;
            
            // Rafraîchir l'UI principale seulement si des changements ont été faits
            if (hasChanges[0]) {
                refreshMainUI();
            }
        });
        
        // Fermer le dialog en cliquant sur le fond noir
        if (popupBackground != null) {
            popupBackground.setOnClickListener(v -> {
                // Supprimer le fond noir immédiatement
                if (popupBackground != null && popupBackground.getParent() instanceof android.view.ViewGroup) {
                    ((android.view.ViewGroup) popupBackground.getParent()).removeView(popupBackground);
                    popupBackground = null;
                }
                dialog.dismiss();
            });
        }
        
        // Configurer la taille du dialog (largeur pleine avec marges de 20dp comme les cards)
        int cardSideMargin = context.getResources().getDimensionPixelSize(R.dimen.card_side_margin);
        int screenWidth = context.getResources().getDisplayMetrics().widthPixels;
        dialog.getWindow().setLayout(
            screenWidth - (2 * cardSideMargin),
            android.view.ViewGroup.LayoutParams.WRAP_CONTENT
        );
        
        // Afficher le dialog
        dialog.show();
    }
}
