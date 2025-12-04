package com.van.management.ui.activities;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.van.management.R;

import java.util.List;

/**
 * Activité pour éditer un mode de lumière
 * Permet de modifier le nom et d'assigner le mode à une position de l'interrupteur physique
 */
public class LightEditorActivity extends AppCompatActivity {
    private static final String TAG = "LightEditorActivity";
    
    public static final String EXTRA_MODE_NAME = "mode_name";
    public static final String EXTRA_MODE_NUMBER = "mode_number";
    public static final String EXTRA_IS_INTERIOR = "is_interior";
    public static final String EXTRA_MODE_INDEX = "mode_index";
    
    private EditText modeNameInput;
    private Spinner modeNumberSpinner;
    private Spinner virtualNumberSpinner;
    private TextView backButton;
    private View saveButton;
    private View cancelButton;
    private TextView helpButton;
    private TextView descriptionText;
    private View physicalSwitchButton;
    private View virtualButton;
    private View physicalSwitchColumn;
    private View virtualColumn;
    private TextView modePositionLabel;
    private TextView virtualPositionLabel;
    
    // LED Strips
    private android.widget.LinearLayout stripsContainer;
    private View addStripButton;
    private TextView addStripText;
    private java.util.ArrayList<LedStripEditor> stripEditors = new java.util.ArrayList<>();
    private int ledCountPerStrip = 120; // Nombre de LEDs par strip
    
    private String originalModeName;
    private int originalModeNumber;
    private boolean isInterior;
    private int modeIndex;
    private int totalModes = 20; // Total des modes disponibles
    private boolean isPhysicalMode = true; // true = Physical Switch, false = Virtual
    private boolean isDescriptionVisible = false;
    private com.van.management.data.LedModeConfig existingLedConfig; // Config existante pour édition
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_light_editor);
        
        // Récupérer les données passées par l'intent
        Intent intent = getIntent();
        originalModeName = intent.getStringExtra(EXTRA_MODE_NAME);
        originalModeNumber = intent.getIntExtra(EXTRA_MODE_NUMBER, 2);
        isInterior = intent.getBooleanExtra(EXTRA_IS_INTERIOR, true);
        modeIndex = intent.getIntExtra(EXTRA_MODE_INDEX, -1);
        
        // Charger la configuration LED existante si disponible
        String existingConfigJson = intent.getStringExtra("EXISTING_LED_CONFIG");
        if (existingConfigJson != null) {
            try {
                existingLedConfig = new com.google.gson.Gson().fromJson(existingConfigJson, com.van.management.data.LedModeConfig.class);
                Log.d(TAG, "Loaded existing LED config for editing");
            } catch (Exception e) {
                Log.e(TAG, "Error loading existing LED config", e);
            }
        }
        
        Log.d(TAG, "Editing mode: " + originalModeName + " (number: " + originalModeNumber + 
            ", interior: " + isInterior + ", index: " + modeIndex + ")");
        
        // Initialiser les vues
        initViews();
        
        // Remplir les valeurs actuelles
        populateValues();
        
        // Charger les couleurs LED si config existante
        if (existingLedConfig != null) {
            loadLedConfigIntoStrips();
        }
    }
    
    private void initViews() {
        modeNameInput = findViewById(R.id.mode_name_input);
        modeNumberSpinner = findViewById(R.id.mode_number_spinner);
        virtualNumberSpinner = findViewById(R.id.virtual_number_spinner);
        backButton = findViewById(R.id.back_button);
        saveButton = findViewById(R.id.save_button);
        cancelButton = findViewById(R.id.cancel_button);
        helpButton = findViewById(R.id.help_button);
        descriptionText = findViewById(R.id.mode_number_description);
        physicalSwitchButton = findViewById(R.id.physical_switch_button);
        virtualButton = findViewById(R.id.virtual_button);
        physicalSwitchColumn = findViewById(R.id.physical_switch_column);
        virtualColumn = findViewById(R.id.virtual_column);
        modePositionLabel = findViewById(R.id.mode_position_label);
        virtualPositionLabel = findViewById(R.id.virtual_position_label);
        
        // LED Strips
        stripsContainer = findViewById(R.id.strips_container);
        addStripButton = findViewById(R.id.add_strip_button);
        addStripText = findViewById(R.id.add_strip_text);
        
        // Configurer le bouton help pour afficher/masquer la description
        helpButton.setOnClickListener(v -> {
            isDescriptionVisible = !isDescriptionVisible;
            descriptionText.setVisibility(isDescriptionVisible ? View.VISIBLE : View.GONE);
        });
        
        // Configurer les boutons Physical/Virtual toggle
        physicalSwitchButton.setOnClickListener(v -> {
            setModeType(true);
        });
        
        virtualButton.setOnClickListener(v -> {
            setModeType(false);
        });
        
        // Déterminer le type initial (Physical si <= 6, Virtual si > 6)
        isPhysicalMode = (originalModeNumber <= 6);
        
        // Configurer les spinners avec les positions disponibles
        setupPhysicalSpinner();
        setupVirtualSpinner();
        
        // Mettre à jour l'UI du toggle
        updateToggleUI();
        
        // Bouton retour
        backButton.setOnClickListener(v -> {
            finish();
        });
        
        // Bouton Cancel
        cancelButton.setOnClickListener(v -> {
            finish(); // Fermer sans sauvegarder
        });
        
        // Bouton Save
        saveButton.setOnClickListener(v -> {
            saveChanges();
        });
        
        // Configurer les strips LED
        setupLedStrips();
    }
    
    private void setupLedStrips() {
        // Ajouter le premier strip par défaut
        addStrip();
        
        // Configurer le bouton "Add Strip"
        addStripButton.setOnClickListener(v -> {
            if (stripEditors.size() < 2) {
                addStrip();
            }
        });
    }
    
    private void addStrip() {
        int stripNumber = stripEditors.size() + 1;
        LedStripEditor stripEditor = new LedStripEditor(this, stripNumber, ledCountPerStrip);
        
        // Ajouter le listener de suppression
        stripEditor.setOnStripRemovedListener(editor -> {
            removeStrip(editor);
        });
        
        // Ajouter le listener de copie (pour le strip 2)
        if (stripNumber == 2) {
            stripEditor.setOnCopyStripListener(copy -> {
                if (copy && stripEditors.size() >= 1) {
                    // Copier les couleurs du strip 1
                    stripEditor.copyFrom(stripEditors.get(0));
                    Log.d(TAG, "Copied strip 1 colors to strip 2");
                }
            });
        }
        
        stripEditors.add(stripEditor);
        stripsContainer.addView(stripEditor.getView());
        
        // Mettre à jour l'état du bouton Add Strip
        updateAddStripButton();
    }
    
    private void removeStrip(LedStripEditor editor) {
        stripEditors.remove(editor);
        stripsContainer.removeView(editor.getView());
        
        // Mettre à jour l'état du bouton Add Strip
        updateAddStripButton();
    }
    
    private void updateAddStripButton() {
        if (stripEditors.size() >= 2) {
            addStripButton.setEnabled(false);
            addStripButton.setAlpha(0.5f);
            addStripText.setText("Max 2 strips");
        } else {
            addStripButton.setEnabled(true);
            addStripButton.setAlpha(1.0f);
            addStripText.setText("➕ Add Strip");
        }
    }
    
    private void setModeType(boolean isPhysical) {
        isPhysicalMode = isPhysical;
        updateToggleUI();
        
        // Griser/activer uniquement les labels et spinners (pas les boutons toggle ni les colonnes)
        if (isPhysicalMode) {
            // Activer Physical, griser Virtual
            modePositionLabel.setAlpha(1.0f);
            modePositionLabel.setEnabled(true);
            modeNumberSpinner.setAlpha(1.0f);
            modeNumberSpinner.setEnabled(true);
            
            virtualPositionLabel.setAlpha(0.5f);
            virtualPositionLabel.setEnabled(false);
            virtualNumberSpinner.setAlpha(0.5f);
            virtualNumberSpinner.setEnabled(false);
        } else {
            // Activer Virtual, griser Physical
            modePositionLabel.setAlpha(0.5f);
            modePositionLabel.setEnabled(false);
            modeNumberSpinner.setAlpha(0.5f);
            modeNumberSpinner.setEnabled(false);
            
            virtualPositionLabel.setAlpha(1.0f);
            virtualPositionLabel.setEnabled(true);
            virtualNumberSpinner.setAlpha(1.0f);
            virtualNumberSpinner.setEnabled(true);
        }
    }
    
    private void updateToggleUI() {
        // Les deux boutons restent toujours actifs visuellement
        physicalSwitchButton.setAlpha(1.0f);
        physicalSwitchButton.setEnabled(true);
        virtualButton.setAlpha(1.0f);
        virtualButton.setEnabled(true);
        
        // Appliquer la couleur button_selected au bouton sélectionné
        if (isPhysicalMode) {
            physicalSwitchButton.setSelected(true);
            virtualButton.setSelected(false);
        } else {
            physicalSwitchButton.setSelected(false);
            virtualButton.setSelected(true);
        }
    }
    
    private void setupPhysicalSpinner() {
        // Position 1 est réservée pour OFF, donc 2-6 pour les autres modes
        String[] positions;
        if (originalModeNumber == 1) {
            // Pour le mode OFF (position 1), on ne peut pas changer
            positions = new String[]{"1 (OFF - Fixed)"};
            modeNumberSpinner.setEnabled(false);
            isPhysicalMode = true; // OFF est toujours physical
        } else {
            // Pour tous les autres modes, on peut choisir 2-6
            positions = new String[]{"2", "3", "4", "5", "6"};
        }
        
        ArrayAdapter<String> adapter = new ArrayAdapter<>(
            this, 
            R.layout.spinner_item, 
            positions
        );
        adapter.setDropDownViewResource(R.layout.spinner_dropdown_item);
        modeNumberSpinner.setAdapter(adapter);
    }
    
    private void setupVirtualSpinner() {
        // Positions virtuelles de 7 au nombre total de modes
        int virtualModeCount = totalModes - 6; // 6 positions physiques, le reste est virtuel
        String[] positions = new String[virtualModeCount];
        for (int i = 0; i < virtualModeCount; i++) {
            positions[i] = String.valueOf(7 + i);
        }
        
        ArrayAdapter<String> adapter = new ArrayAdapter<>(
            this, 
            R.layout.spinner_item, 
            positions
        );
        adapter.setDropDownViewResource(R.layout.spinner_dropdown_item);
        virtualNumberSpinner.setAdapter(adapter);
    }
    
    private void populateValues() {
        // Remplir le nom du mode
        modeNameInput.setText(originalModeName);
        
        // Cacher les spinners de sélection de numéro (assigné automatiquement)
        if (physicalSwitchColumn != null) physicalSwitchColumn.setVisibility(View.GONE);
        if (virtualColumn != null) virtualColumn.setVisibility(View.GONE);
        if (physicalSwitchButton != null) physicalSwitchButton.setVisibility(View.GONE);
        if (virtualButton != null) virtualButton.setVisibility(View.GONE);
        
        // Afficher le numéro assigné automatiquement
        if (descriptionText != null) {
            descriptionText.setVisibility(View.VISIBLE);
            descriptionText.setText("Mode number: " + originalModeNumber + " (assigned automatically based on position)");
        }
    }
    
    private void saveChanges() {
        String newName = modeNameInput.getText().toString().trim();
        
        // Validation
        if (newName.isEmpty()) {
            Toast.makeText(this, "Please enter a mode name", Toast.LENGTH_SHORT).show();
            return;
        }
        
        // Le numéro est assigné automatiquement = pas de changement
        // Il reste le même que originalModeNumber
        int newNumber = originalModeNumber;
        
        // Créer la configuration LED à partir des strips
        com.van.management.data.LedModeConfig ledConfig = createLedModeConfig();
        
        // Créer l'intent de résultat
        Intent resultIntent = new Intent();
        resultIntent.putExtra(EXTRA_MODE_NAME, newName);
        resultIntent.putExtra(EXTRA_MODE_INDEX, modeIndex);
        resultIntent.putExtra("LED_CONFIG", new com.google.gson.Gson().toJson(ledConfig));
        
        setResult(RESULT_OK, resultIntent);
        
        Log.d(TAG, "Saving changes: " + newName + " at index " + modeIndex + " (number: " + newNumber + ")");
        
        finish();
    }
    
    /**
     * Créer un LedModeConfig à partir des strips configurés
     */
    private com.van.management.data.LedModeConfig createLedModeConfig() {
        if (stripEditors.isEmpty()) {
            return new com.van.management.data.LedModeConfig(); // Config vide
        }
        
        // Obtenir les données RGBW du strip 1
        int[][] strip1RGBW = stripEditors.get(0).getRGBWValues();
        com.van.management.data.LedStripData strip1Data = new com.van.management.data.LedStripData();
        for (int i = 0; i < strip1RGBW.length; i++) {
            strip1Data.setLed(i, strip1RGBW[i][0], strip1RGBW[i][1], strip1RGBW[i][2], strip1RGBW[i][3]);
        }
        
        // Vérifier si on doit copier strip1 ou utiliser strip2
        boolean copyStrip1 = false;
        com.van.management.data.LedStripData strip2Data = null;
        
        if (stripEditors.size() >= 2) {
            // Vérifier si le strip 2 est configuré en mode copie
            copyStrip1 = stripEditors.get(1).isCopyingStrip1();
            
            if (copyStrip1) {
                // Mode copie : strip2Data reste null, on copiera strip1 dynamiquement
                strip2Data = null;
            } else {
                // 2 strips différents : obtenir les données RGBW du strip 2
                int[][] strip2RGBW = stripEditors.get(1).getRGBWValues();
                strip2Data = new com.van.management.data.LedStripData();
                for (int i = 0; i < strip2RGBW.length; i++) {
                    strip2Data.setLed(i, strip2RGBW[i][0], strip2RGBW[i][1], strip2RGBW[i][2], strip2RGBW[i][3]);
                }
            }
        } else {
            // Un seul strip = strip2Data reste null, copyStrip1 reste false
            copyStrip1 = false;
            strip2Data = null;
        }
        
        Log.d(TAG, "createLedModeConfig: stripCount=" + stripEditors.size() + 
              ", copyStrip1=" + copyStrip1 + ", strip2Data=" + (strip2Data != null ? "not null" : "null"));
        
        return new com.van.management.data.LedModeConfig(strip1Data, strip2Data, copyStrip1);
    }
    
    /**
     * Charger la configuration LED existante dans les strips pour édition
     */
    private void loadLedConfigIntoStrips() {
        if (existingLedConfig == null || stripEditors.isEmpty()) {
            return;
        }
        
        try {
            // Charger strip 1
            com.van.management.data.LedStripData strip1Data = existingLedConfig.getStrip1Data();
            if (strip1Data != null && stripEditors.size() >= 1) {
                List<Integer> strip1Colors = strip1Data.getDisplayColors();
                stripEditors.get(0).setLedColors(strip1Colors);
                Log.d(TAG, "Loaded strip 1 colors: " + strip1Colors.size() + " LEDs");
            }
            
            // Vérifier si on a besoin d'ajouter le strip 2
            boolean needsStrip2 = !existingLedConfig.isCopyStrip1() || existingLedConfig.getStrip2Data() != null;
            
            if (needsStrip2 && stripEditors.size() < 2) {
                // Ajouter automatiquement le strip 2 s'il n'existe pas encore
                Log.d(TAG, "Adding strip 2 automatically because ledConfig has 2 strips");
                addStrip();
            }
            
            // Charger strip 2 si présent
            if (stripEditors.size() >= 2) {
                if (existingLedConfig.isCopyStrip1()) {
                    // Activer le mode copie
                    stripEditors.get(1).setCopyStrip1(true);
                    Log.d(TAG, "Strip 2 set to copy mode");
                } else {
                    com.van.management.data.LedStripData strip2Data = existingLedConfig.getStrip2Data();
                    if (strip2Data != null) {
                        List<Integer> strip2Colors = strip2Data.getDisplayColors();
                        stripEditors.get(1).setLedColors(strip2Colors);
                        Log.d(TAG, "Loaded strip 2 colors: " + strip2Colors.size() + " LEDs");
                    }
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Error loading LED config into strips", e);
        }
    }
    
    @Override
    public void onBackPressed() {
        // Permettre le retour arrière sans sauvegarder
        super.onBackPressed();
    }
}
