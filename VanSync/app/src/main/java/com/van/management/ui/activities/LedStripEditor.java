package com.van.management.ui.activities;

import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.van.management.R;
import com.van.management.ui.views.LedStripCanvas;

import java.util.List;

/**
 * Classe pour gérer un strip LED dans l'interface d'édition
 */
public class LedStripEditor {
    private Context context;
    private View stripView;
    private LedStripCanvas stripCanvas;
    private View stripSettingsContainer;
    private CheckBox copyStripCheckbox;
    private TextView stripNameLabel;
    private TextView stripRemoveButton;
    private View resetButton;
    private View divideButton;
    private View gradientButton;
    private LinearLayout colorPalette;
    private android.widget.SeekBar brushWidthSlider;
    private View brushPreview;
    private android.widget.Button zoomInButton;
    private android.widget.Button zoomOutButton;
    private android.widget.SeekBar stripScrollbar;
    
    private int stripNumber;
    private int ledCount;
    private int selectedColorIndex = 0;
    private int brushWidth = 1; // Largeur du crayon en nombre de LEDs
    
    // Constantes pour la couleur White 4000K
    private static final int WHITE_4000K_R = 255;
    private static final int WHITE_4000K_G = 234;
    private static final int WHITE_4000K_B = 224;
    private static final float WHITE_4000K_RATIO_R = 1.0f;
    private static final float WHITE_4000K_RATIO_G = WHITE_4000K_G / 255.0f; // 0.917647
    private static final float WHITE_4000K_RATIO_B = WHITE_4000K_B / 255.0f; // 0.839216
    
    // Classe pour stocker les couleurs RGBW
    private static class RGBWColor {
        int r, g, b, w;
        
        RGBWColor(int r, int g, int b, int w) {
            this.r = r;
            this.g = g;
            this.b = b;
            this.w = w;
        }
        
        // Obtenir la couleur d'affichage (RGB + mélange W)
        int getDisplayColor() {
            int mixedR = Math.min(255, r + (int)(w * WHITE_4000K_RATIO_R));
            int mixedG = Math.min(255, g + (int)(w * WHITE_4000K_RATIO_G));
            int mixedB = Math.min(255, b + (int)(w * WHITE_4000K_RATIO_B));
            return Color.rgb(mixedR, mixedG, mixedB);
        }
    }
    
    // Palette de couleurs prédéfinies avec RGBW (6 couleurs)
    // Les 2 premières sont fixes (non éditables)
    private static final RGBWColor[] PRESET_COLORS = {
        new RGBWColor(0, 0, 0, 0),        // Black (Off) - FIXE
        new RGBWColor(0, 0, 0, 255),      // Natural White 4000K - FIXE
        new RGBWColor(255, 0, 0, 0),      // Red - Éditable
        new RGBWColor(0, 255, 0, 0),      // Green - Éditable
        new RGBWColor(0, 0, 255, 0),      // Blue - Éditable
        new RGBWColor(255, 255, 255, 0)   // White RGB - Éditable
    };
    
    public interface OnStripRemovedListener {
        void onStripRemoved(LedStripEditor editor);
    }
    
    public interface OnCopyStripListener {
        void onCopyRequested(boolean copy);
    }
    
    private OnStripRemovedListener removeListener;
    private OnCopyStripListener copyListener;
    
    public LedStripEditor(Context context, int stripNumber, int ledCount) {
        this.context = context;
        this.stripNumber = stripNumber;
        this.ledCount = ledCount;
        
        LayoutInflater inflater = LayoutInflater.from(context);
        stripView = inflater.inflate(R.layout.led_strip_color_editor, null);
        
        initViews();
        setupListeners();
        createColorPalette();
    }
    
    private void initViews() {
        stripCanvas = stripView.findViewById(R.id.led_strip_canvas);
        stripSettingsContainer = stripView.findViewById(R.id.strip_settings_container);
        copyStripCheckbox = stripView.findViewById(R.id.copy_strip_checkbox);
        stripNameLabel = stripView.findViewById(R.id.strip_name_label);
        stripRemoveButton = stripView.findViewById(R.id.strip_remove_button);
        resetButton = stripView.findViewById(R.id.reset_button);
        divideButton = stripView.findViewById(R.id.divide_button);
        gradientButton = stripView.findViewById(R.id.gradient_button);
        colorPalette = stripView.findViewById(R.id.color_palette);
        brushWidthSlider = stripView.findViewById(R.id.brush_width_slider);
        brushPreview = stripView.findViewById(R.id.brush_preview);
        zoomInButton = stripView.findViewById(R.id.zoom_in_button);
        zoomOutButton = stripView.findViewById(R.id.zoom_out_button);
        stripScrollbar = stripView.findViewById(R.id.strip_scrollbar);
        
        // Configurer le strip
        stripCanvas.setLedCount(ledCount);
        stripNameLabel.setText("Strip " + stripNumber);
        
        // Configurer l'aperçu du crayon
        updateBrushPreview();
        
        // Initialiser l'état des contrôles de zoom
        updateScrollbar();
        
        // Afficher le bouton remove seulement pour le strip 2
        if (stripNumber == 2) {
            stripRemoveButton.setVisibility(View.VISIBLE);
            copyStripCheckbox.setVisibility(View.VISIBLE);
        }
    }
    
    private void setupListeners() {
        // Reset button
        resetButton.setOnClickListener(v -> {
            stripCanvas.reset();
        });
        
        // Divide button
        divideButton.setOnClickListener(v -> {
            showDivideDialog();
        });
        
        // Gradient button
        gradientButton.setOnClickListener(v -> {
            stripCanvas.applyGradient();
        });
        
        // Remove button
        stripRemoveButton.setOnClickListener(v -> {
            if (removeListener != null) {
                removeListener.onStripRemoved(this);
            }
        });
        
        // Copy checkbox
        copyStripCheckbox.setOnCheckedChangeListener((buttonView, isChecked) -> {
            stripSettingsContainer.setVisibility(isChecked ? View.GONE : View.VISIBLE);
            
            if (isChecked && copyListener != null) {
                // Copier immédiatement les couleurs du strip 1
                copyListener.onCopyRequested(true);
            }
        });
        
        // Listener pour détecter les modifications sur le strip canvas (pour strip 2 seulement)
        if (stripNumber == 2) {
            stripCanvas.setOnStripChangedListener(colors -> {
                // Si la checkbox est cochée et que l'utilisateur modifie le strip 2,
                // décocher automatiquement la checkbox
                if (copyStripCheckbox != null && copyStripCheckbox.isChecked()) {
                    android.util.Log.d("LedStripEditor", "Strip 2 modified while copy mode active - unchecking copy checkbox");
                    copyStripCheckbox.setChecked(false);
                }
            });
        }
        
        // Brush width slider
        brushWidthSlider.setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(android.widget.SeekBar seekBar, int progress, boolean fromUser) {
                brushWidth = progress;
                stripCanvas.setBrushWidth(brushWidth);
                updateBrushPreview();
            }
            
            @Override
            public void onStartTrackingTouch(android.widget.SeekBar seekBar) {}
            
            @Override
            public void onStopTrackingTouch(android.widget.SeekBar seekBar) {}
        });
        
        // Zoom buttons
        zoomInButton.setOnClickListener(v -> {
            float currentZoom = stripCanvas.getZoomLevel();
            stripCanvas.setZoomLevel(Math.min(10.0f, currentZoom + 1.0f));
            updateScrollbar();
        });
        
        zoomOutButton.setOnClickListener(v -> {
            float currentZoom = stripCanvas.getZoomLevel();
            stripCanvas.setZoomLevel(Math.max(1.0f, currentZoom - 1.0f));
            updateScrollbar();
        });
        
        // Strip scrollbar
        stripScrollbar.setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(android.widget.SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    stripCanvas.setScrollPosition(progress / 100.0f);
                }
            }
            
            @Override
            public void onStartTrackingTouch(android.widget.SeekBar seekBar) {}
            
            @Override
            public void onStopTrackingTouch(android.widget.SeekBar seekBar) {}
        });
    }
    
    private void updateScrollbar() {
        float zoom = stripCanvas.getZoomLevel();
        if (zoom > 1.0f) {
            stripScrollbar.setVisibility(View.VISIBLE);
        } else {
            stripScrollbar.setVisibility(View.GONE);
            stripScrollbar.setProgress(0);
        }
        
        // Désactiver les boutons de zoom aux limites
        zoomOutButton.setEnabled(zoom > 1.0f);
        zoomOutButton.setAlpha(zoom > 1.0f ? 1.0f : 0.3f);
        
        zoomInButton.setEnabled(zoom < 10.0f);
        zoomInButton.setAlpha(zoom < 10.0f ? 1.0f : 0.3f);
    }
    
    private void createColorPalette() {
        colorPalette.removeAllViews();
        
        int colorButtonSize = (int) (40 * context.getResources().getDisplayMetrics().density);
        int margin = (int) (4 * context.getResources().getDisplayMetrics().density);
        
        for (int i = 0; i < PRESET_COLORS.length; i++) {
            final int colorIndex = i;
            final RGBWColor rgbwColor = PRESET_COLORS[i];
            final int displayColor = rgbwColor.getDisplayColor();
            final boolean isFixed = (i < 2); // Les 2 premiers sont fixes
            
            // Créer un cercle de couleur
            View colorButton = new View(context);
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                colorButtonSize, colorButtonSize
            );
            params.setMargins(margin, margin, margin, margin);
            colorButton.setLayoutParams(params);
            
            // Créer un drawable circulaire avec la couleur
            android.graphics.drawable.GradientDrawable circle = new android.graphics.drawable.GradientDrawable();
            circle.setShape(android.graphics.drawable.GradientDrawable.OVAL);
            circle.setColor(displayColor);
            
            // Stroke: Orange pour les fixes, blanc pour sélection, transparent sinon
            int strokeColor;
            if (isFixed) {
                strokeColor = Color.parseColor("#FF8800"); // Orange pour les couleurs fixes
            } else if (i == selectedColorIndex) {
                strokeColor = Color.WHITE; // Blanc pour la sélection
            } else {
                strokeColor = Color.TRANSPARENT;
            }
            circle.setStroke(3, strokeColor);
            colorButton.setBackground(circle);
            
            // Effet de sélection (pas d'effet alpha pour les fixes)
            if (isFixed) {
                colorButton.setAlpha(1.0f);
            } else {
                colorButton.setAlpha(i == selectedColorIndex ? 1.0f : 0.7f);
            }
            
            // Click court : sélectionner la couleur
            colorButton.setOnClickListener(v -> {
                selectColor(colorIndex);
            });
            
            // Long click : ouvrir le color picker SEULEMENT pour les couleurs éditables
            if (!isFixed) {
                colorButton.setOnLongClickListener(v -> {
                    showColorPickerDialog(colorIndex);
                    return true;
                });
            }
            
            colorPalette.addView(colorButton);
        }
    }
    
    private void updateBrushPreview() {
        // Créer un cercle avec la couleur sélectionnée et une taille proportionnelle à brushWidth
        int baseSize = (int) (10 * context.getResources().getDisplayMetrics().density);
        int size = baseSize + (int) (brushWidth * 1.5 * context.getResources().getDisplayMetrics().density);
        
        // Limiter la taille maximale
        size = Math.min(size, (int) (30 * context.getResources().getDisplayMetrics().density));
        
        android.view.ViewGroup.LayoutParams params = brushPreview.getLayoutParams();
        params.width = size;
        params.height = size;
        brushPreview.setLayoutParams(params);
        
        android.graphics.drawable.GradientDrawable circle = new android.graphics.drawable.GradientDrawable();
        circle.setShape(android.graphics.drawable.GradientDrawable.OVAL);
        circle.setColor(PRESET_COLORS[selectedColorIndex].getDisplayColor());
        circle.setStroke(3, Color.WHITE);
        brushPreview.setBackground(circle);
    }
    
    private void selectColor(int colorIndex) {
        selectedColorIndex = colorIndex;
        stripCanvas.setSelectedColor(PRESET_COLORS[colorIndex].getDisplayColor());
        updateBrushPreview(); // Mettre à jour l'aperçu avec la nouvelle couleur
        
        // Mettre à jour l'apparence des boutons de couleur
        for (int i = 0; i < colorPalette.getChildCount(); i++) {
            View child = colorPalette.getChildAt(i);
            boolean isFixed = (i < 2); // Les 2 premiers sont fixes
            
            // Alpha: toujours 1.0 pour les fixes, sinon selon sélection
            if (isFixed) {
                child.setAlpha(1.0f);
            } else {
                child.setAlpha(i == colorIndex ? 1.0f : 0.7f);
            }
            
            // Mettre à jour la bordure
            if (child.getBackground() instanceof android.graphics.drawable.GradientDrawable) {
                android.graphics.drawable.GradientDrawable circle = 
                    (android.graphics.drawable.GradientDrawable) child.getBackground();
                
                // Stroke: Orange pour les fixes, blanc pour sélection, transparent sinon
                int strokeColor;
                if (isFixed) {
                    strokeColor = Color.parseColor("#FF8800"); // Orange pour les couleurs fixes
                } else if (i == colorIndex) {
                    strokeColor = Color.WHITE; // Blanc pour la sélection
                } else {
                    strokeColor = Color.TRANSPARENT;
                }
                circle.setStroke(3, strokeColor);
            }
        }
    }
    
    private void showColorPickerDialog(int colorIndex) {
        final RGBWColor currentRGBW = PRESET_COLORS[colorIndex];
        final int currentColor = currentRGBW.getDisplayColor();
        
        // Créer un fond noir transparent pour cacher l'interface
        final View popupBackground = new View(context);
        popupBackground.setLayoutParams(new android.widget.FrameLayout.LayoutParams(
            android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
            android.widget.FrameLayout.LayoutParams.MATCH_PARENT
        ));
        popupBackground.setBackgroundColor(android.graphics.Color.argb(220, 0, 0, 0)); // Noir à 86% d'opacité
        
        // Ajouter le fond au niveau de la racine de l'activité
        ViewGroup rootActivity = (ViewGroup) ((android.app.Activity) context).getWindow().getDecorView().findViewById(android.R.id.content);
        if (rootActivity != null) {
            rootActivity.addView(popupBackground);
        }
        
        // Créer une vue personnalisée pour le color picker
        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(40, 40, 40, 40);
        
        // Créer un drawable avec coins arrondis pour le fond
        android.graphics.drawable.GradientDrawable backgroundDrawable = 
            new android.graphics.drawable.GradientDrawable();
        backgroundDrawable.setColor(Color.parseColor("#1E1E1E")); // Fond sombre comme l'app
        backgroundDrawable.setCornerRadius(20 * context.getResources().getDisplayMetrics().density);
        backgroundDrawable.setStroke(
            (int)(2 * context.getResources().getDisplayMetrics().density),
            Color.WHITE // Bordure blanche
        );
        layout.setBackground(backgroundDrawable);
        
        // Layout horizontal pour la Color Wheel + Slider White vertical
        LinearLayout wheelContainer = new LinearLayout(context);
        wheelContainer.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams wheelContainerParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        wheelContainerParams.gravity = android.view.Gravity.CENTER;
        wheelContainerParams.setMargins(0, 20, 0, 30);
        wheelContainer.setLayoutParams(wheelContainerParams);
        
        // Color Wheel
        final com.van.management.ui.views.ColorWheelView colorWheel = 
            new com.van.management.ui.views.ColorWheelView(context);
        int wheelSize = (int) (250 * context.getResources().getDisplayMetrics().density);
        LinearLayout.LayoutParams wheelParams = new LinearLayout.LayoutParams(wheelSize, wheelSize);
        wheelParams.setMargins(0, 0, 20, 0);
        colorWheel.setLayoutParams(wheelParams);
        // Afficher seulement la partie RGB (sans W) dans le wheel
        colorWheel.setColor(Color.rgb(currentRGBW.r, currentRGBW.g, currentRGBW.b));
        wheelContainer.addView(colorWheel);
        
        // Slider White vertical personnalisé (déclaré comme tableau pour être utilisé dans les listeners)
        final com.van.management.ui.views.VerticalWhiteSlider[] whiteSliderVerticalRef = 
            new com.van.management.ui.views.VerticalWhiteSlider[1];
        whiteSliderVerticalRef[0] = new com.van.management.ui.views.VerticalWhiteSlider(context);
        final com.van.management.ui.views.VerticalWhiteSlider whiteSliderVertical = whiteSliderVerticalRef[0];
        LinearLayout.LayoutParams whiteSliderParams = new LinearLayout.LayoutParams(
            (int) (30 * context.getResources().getDisplayMetrics().density),
            wheelSize
        );
        whiteSliderParams.setMargins(
            (int) (10 * context.getResources().getDisplayMetrics().density),
            0, 0, 0
        );
        whiteSliderVertical.setLayoutParams(whiteSliderParams);
        whiteSliderVertical.setWhiteValue(currentRGBW.w);
        
        wheelContainer.addView(whiteSliderVertical);
        layout.addView(wheelContainer);
        
        // Preview de la couleur
        final View colorPreview = new View(context);
        int previewSize = (int) (60 * context.getResources().getDisplayMetrics().density);
        LinearLayout.LayoutParams previewParams = new LinearLayout.LayoutParams(previewSize, previewSize);
        previewParams.gravity = android.view.Gravity.CENTER;
        previewParams.setMargins(0, 0, 0, 30);
        colorPreview.setLayoutParams(previewParams);
        
        final android.graphics.drawable.GradientDrawable previewCircle = 
            new android.graphics.drawable.GradientDrawable();
        previewCircle.setShape(android.graphics.drawable.GradientDrawable.OVAL);
        previewCircle.setColor(currentColor);
        previewCircle.setStroke(3, Color.WHITE);
        colorPreview.setBackground(previewCircle);
        layout.addView(colorPreview);
        
        // Sliders et TextBoxes RGBW
        final android.widget.SeekBar[] rgbwSeekBars = new android.widget.SeekBar[4];
        final android.widget.EditText[] rgbwEditTexts = new android.widget.EditText[4];
        String[] labels = {"Red", "Green", "Blue", "Natural White"};
        int[] initialValues = {
            currentRGBW.r, 
            currentRGBW.g, 
            currentRGBW.b,
            currentRGBW.w // Charger la valeur W sauvegardée
        };
        
        // Flag pour éviter les boucles infinies lors des mises à jour
        final boolean[] isUpdating = {false};
        
        for (int i = 0; i < 4; i++) {
            final int index = i;
            
            // Layout horizontal pour label + edittext
            LinearLayout rowLayout = new LinearLayout(context);
            rowLayout.setOrientation(LinearLayout.HORIZONTAL);
            rowLayout.setPadding(0, 10, 0, 5);
            
            android.widget.TextView label = new android.widget.TextView(context);
            label.setText(labels[i] + ": ");
            label.setTextColor(Color.WHITE);
            label.setTextSize(16);
            label.setLayoutParams(new LinearLayout.LayoutParams(
                (int) (120 * context.getResources().getDisplayMetrics().density),
                LinearLayout.LayoutParams.WRAP_CONTENT
            ));
            rowLayout.addView(label);
            
            // EditText pour la valeur
            rgbwEditTexts[i] = new android.widget.EditText(context);
            rgbwEditTexts[i].setText(String.valueOf(initialValues[i]));
            rgbwEditTexts[i].setTextColor(Color.WHITE);
            rgbwEditTexts[i].setInputType(android.text.InputType.TYPE_CLASS_NUMBER);
            rgbwEditTexts[i].setLayoutParams(new LinearLayout.LayoutParams(
                (int) (80 * context.getResources().getDisplayMetrics().density),
                LinearLayout.LayoutParams.WRAP_CONTENT
            ));
            rgbwEditTexts[i].setBackgroundColor(Color.parseColor("#404040"));
            rgbwEditTexts[i].setPadding(10, 10, 10, 10);
            rowLayout.addView(rgbwEditTexts[i]);
            
            layout.addView(rowLayout);
            
            // Slider
            rgbwSeekBars[i] = new android.widget.SeekBar(context);
            rgbwSeekBars[i].setMax(255);
            rgbwSeekBars[i].setProgress(initialValues[i]);
            LinearLayout.LayoutParams sliderParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            );
            sliderParams.setMargins(0, 5, 0, 15);
            rgbwSeekBars[i].setLayoutParams(sliderParams);
            layout.addView(rgbwSeekBars[i]);
            
            // Listener pour le slider (sera redéfini pour le White slider après)
            if (i < 3) {
                // Pour RGB seulement
                rgbwSeekBars[i].setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener() {
                    @Override
                    public void onProgressChanged(android.widget.SeekBar seekBar, int progress, boolean fromUser) {
                        if (!fromUser || isUpdating[0]) return;
                        
                        isUpdating[0] = true;
                        
                        // Mettre à jour l'EditText
                        rgbwEditTexts[index].setText(String.valueOf(progress));
                        
                        // Mettre à jour la couleur avec mélange RGBW
                        int r = rgbwSeekBars[0].getProgress();
                        int g = rgbwSeekBars[1].getProgress();
                        int b = rgbwSeekBars[2].getProgress();
                        int w = rgbwSeekBars[3].getProgress();
                        
                        // Simuler le white 4000K en mélangeant avec la couleur RGB
                        int mixedR = Math.min(255, r + (int)(w * WHITE_4000K_RATIO_R));
                        int mixedG = Math.min(255, g + (int)(w * WHITE_4000K_RATIO_G));
                        int mixedB = Math.min(255, b + (int)(w * WHITE_4000K_RATIO_B));
                        int displayColor = Color.rgb(mixedR, mixedG, mixedB);
                        
                        // Mettre à jour la preview
                        previewCircle.setColor(displayColor);
                        
                        // Mettre à jour la color wheel pour RGB
                        int baseColor = Color.rgb(r, g, b);
                        colorWheel.setColor(baseColor);
                        
                        isUpdating[0] = false;
                    }
                    
                    @Override
                    public void onStartTrackingTouch(android.widget.SeekBar seekBar) {}
                    
                    @Override
                    public void onStopTrackingTouch(android.widget.SeekBar seekBar) {}
                });
            }
            
            // Listener pour l'EditText
            rgbwEditTexts[i].addTextChangedListener(new android.text.TextWatcher() {
                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
                
                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {}
                
                @Override
                public void afterTextChanged(android.text.Editable s) {
                    if (isUpdating[0]) return;
                    
                    try {
                        String text = s.toString();
                        if (text.isEmpty()) return;
                        
                        int value = Integer.parseInt(text);
                        if (value < 0) value = 0;
                        if (value > 255) value = 255;
                        
                        isUpdating[0] = true;
                        
                        // Mettre à jour le slider
                        rgbwSeekBars[index].setProgress(value);
                        
                        // Pour RGB: mettre à jour aussi le slider vertical White si c'est le W
                        if (index == 3 && whiteSliderVerticalRef[0] != null) {
                            whiteSliderVerticalRef[0].setWhiteValue(value);
                        }
                        
                        // Mettre à jour la couleur avec mélange RGBW
                        int r = rgbwSeekBars[0].getProgress();
                        int g = rgbwSeekBars[1].getProgress();
                        int b = rgbwSeekBars[2].getProgress();
                        int w = rgbwSeekBars[3].getProgress();
                        
                        // Simuler le white 4000K en mélangeant avec la couleur RGB
                        int mixedR = Math.min(255, r + (int)(w * WHITE_4000K_RATIO_R));
                        int mixedG = Math.min(255, g + (int)(w * WHITE_4000K_RATIO_G));
                        int mixedB = Math.min(255, b + (int)(w * WHITE_4000K_RATIO_B));
                        int displayColor = Color.rgb(mixedR, mixedG, mixedB);
                        
                        // Mettre à jour la preview
                        previewCircle.setColor(displayColor);
                        
                        // Ne mettre à jour la color wheel que si ce n'est pas le slider White
                        if (index < 3) {
                            int baseColor = Color.rgb(r, g, b);
                            colorWheel.setColor(baseColor);
                        }
                        
                        isUpdating[0] = false;
                    } catch (NumberFormatException e) {
                        // Ignorer les entrées invalides
                    }
                }
            });
        }
        
        // Listener pour la color wheel
        colorWheel.setOnColorChangedListener(color -> {
            if (isUpdating[0]) return;
            
            isUpdating[0] = true;
            
            // Extraire les composants RGB
            int r = Color.red(color);
            int g = Color.green(color);
            int b = Color.blue(color);
            
            // Mettre à jour les sliders RGB (pas le W)
            rgbwSeekBars[0].setProgress(r);
            rgbwSeekBars[1].setProgress(g);
            rgbwSeekBars[2].setProgress(b);
            
            // Mettre à jour les EditTexts RGB (pas le W)
            rgbwEditTexts[0].setText(String.valueOf(r));
            rgbwEditTexts[1].setText(String.valueOf(g));
            rgbwEditTexts[2].setText(String.valueOf(b));
            
            // Mettre à jour la preview en incluant le white
            int w = rgbwSeekBars[3].getProgress();
            int mixedR = Math.min(255, r + (int)(w * WHITE_4000K_RATIO_R));
            int mixedG = Math.min(255, g + (int)(w * WHITE_4000K_RATIO_G));
            int mixedB = Math.min(255, b + (int)(w * WHITE_4000K_RATIO_B));
            int displayColor = Color.rgb(mixedR, mixedG, mixedB);
            previewCircle.setColor(displayColor);
            
            isUpdating[0] = false;
        });
        
        // Synchroniser le slider vertical avec le slider horizontal
        whiteSliderVertical.setOnWhiteValueChangeListener(value -> {
            if (isUpdating[0]) return;
            
            isUpdating[0] = true;
            
            // Mettre à jour le slider horizontal et l'EditText
            rgbwSeekBars[3].setProgress(value);
            rgbwEditTexts[3].setText(String.valueOf(value));
            
            // Mettre à jour la preview
            int r = rgbwSeekBars[0].getProgress();
            int g = rgbwSeekBars[1].getProgress();
            int b = rgbwSeekBars[2].getProgress();
            int w = value;
            
            int mixedR = Math.min(255, r + (int)(w * WHITE_4000K_RATIO_R));
            int mixedG = Math.min(255, g + (int)(w * WHITE_4000K_RATIO_G));
            int mixedB = Math.min(255, b + (int)(w * WHITE_4000K_RATIO_B));
            int displayColor = Color.rgb(mixedR, mixedG, mixedB);
            previewCircle.setColor(displayColor);
            
            isUpdating[0] = false;
        });
        
        // Synchroniser le slider horizontal White avec le slider vertical
        final android.widget.SeekBar whiteSliderHorizontal = rgbwSeekBars[3];
        whiteSliderHorizontal.setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(android.widget.SeekBar seekBar, int progress, boolean fromUser) {
                if (!fromUser || isUpdating[0]) return;
                
                isUpdating[0] = true;
                
                // Mettre à jour le slider vertical
                whiteSliderVertical.setWhiteValue(progress);
                
                // Mettre à jour l'EditText
                rgbwEditTexts[3].setText(String.valueOf(progress));
                
                // Mettre à jour la couleur avec mélange RGBW
                int r = rgbwSeekBars[0].getProgress();
                int g = rgbwSeekBars[1].getProgress();
                int b = rgbwSeekBars[2].getProgress();
                int w = progress;
                
                int mixedR = Math.min(255, r + (int)(w * WHITE_4000K_RATIO_R));
                int mixedG = Math.min(255, g + (int)(w * WHITE_4000K_RATIO_G));
                int mixedB = Math.min(255, b + (int)(w * WHITE_4000K_RATIO_B));
                int displayColor = Color.rgb(mixedR, mixedG, mixedB);
                
                previewCircle.setColor(displayColor);
                
                isUpdating[0] = false;
            }
            
            @Override
            public void onStartTrackingTouch(android.widget.SeekBar seekBar) {}
            
            @Override
            public void onStopTrackingTouch(android.widget.SeekBar seekBar) {}
        });
        
        // Ajouter les boutons personnalisés dans le layout principal
        LinearLayout buttonsLayout = new LinearLayout(context);
        buttonsLayout.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams buttonsParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        buttonsParams.setMargins(0, 20, 0, 0);
        buttonsLayout.setLayoutParams(buttonsParams);
        buttonsLayout.setGravity(android.view.Gravity.END);
        
        // Bouton Cancel
        android.widget.Button cancelButton = new android.widget.Button(context);
        cancelButton.setText("Cancel");
        LinearLayout.LayoutParams cancelParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        cancelParams.setMargins(0, 0, 20, 0);
        cancelButton.setLayoutParams(cancelParams);
        
        android.graphics.drawable.GradientDrawable cancelButtonBg = 
            new android.graphics.drawable.GradientDrawable();
        cancelButtonBg.setColor(Color.BLACK);
        cancelButtonBg.setCornerRadius(8 * context.getResources().getDisplayMetrics().density);
        cancelButtonBg.setStroke(
            (int)(2 * context.getResources().getDisplayMetrics().density),
            Color.WHITE
        );
        cancelButton.setBackground(cancelButtonBg);
        cancelButton.setTextColor(Color.WHITE);
        cancelButton.setPadding(40, 20, 40, 20);
        cancelButton.setAllCaps(false);
        
        // Bouton OK
        android.widget.Button okButton = new android.widget.Button(context);
        okButton.setText("OK");
        LinearLayout.LayoutParams okParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        okButton.setLayoutParams(okParams);
        
        android.graphics.drawable.GradientDrawable okButtonBg = 
            new android.graphics.drawable.GradientDrawable();
        okButtonBg.setColor(Color.BLACK);
        okButtonBg.setCornerRadius(8 * context.getResources().getDisplayMetrics().density);
        okButtonBg.setStroke(
            (int)(2 * context.getResources().getDisplayMetrics().density),
            Color.WHITE
        );
        okButton.setBackground(okButtonBg);
        okButton.setTextColor(Color.WHITE);
        okButton.setPadding(40, 20, 40, 20);
        okButton.setAllCaps(false);
        
        buttonsLayout.addView(cancelButton);
        buttonsLayout.addView(okButton);
        layout.addView(buttonsLayout);
        
        // Créer le dialog sans boutons par défaut
        AlertDialog dialog = new AlertDialog.Builder(context, android.R.style.Theme_Black_NoTitleBar)
            .setView(layout)
            .create();
        
        // Styliser le dialog
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawableResource(android.R.color.transparent);
            dialog.getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        }
        
        // Gérer la fermeture du dialog pour supprimer le fond
        dialog.setOnDismissListener(d -> {
            if (popupBackground != null && popupBackground.getParent() instanceof ViewGroup) {
                ((ViewGroup) popupBackground.getParent()).removeView(popupBackground);
            }
        });
        
        // Listeners pour les boutons personnalisés
        okButton.setOnClickListener(v -> {
            // Récupérer les valeurs RGBW finales
            int finalR = rgbwSeekBars[0].getProgress();
            int finalG = rgbwSeekBars[1].getProgress();
            int finalB = rgbwSeekBars[2].getProgress();
            int finalW = rgbwSeekBars[3].getProgress();
            
            // Mettre à jour la couleur RGBW dans le tableau
            PRESET_COLORS[colorIndex] = new RGBWColor(finalR, finalG, finalB, finalW);
            
            // Recréer la palette pour afficher la nouvelle couleur
            createColorPalette();
            
            // Sélectionner cette couleur
            selectColor(colorIndex);
            
            // Supprimer le fond noir
            if (popupBackground != null && popupBackground.getParent() instanceof ViewGroup) {
                ((ViewGroup) popupBackground.getParent()).removeView(popupBackground);
            }
            
            dialog.dismiss();
        });
        
        cancelButton.setOnClickListener(v -> {
            // Supprimer le fond noir
            if (popupBackground != null && popupBackground.getParent() instanceof ViewGroup) {
                ((ViewGroup) popupBackground.getParent()).removeView(popupBackground);
            }
            
            dialog.dismiss();
        });
        
        dialog.show();
    }
    
    private void showDivideDialog() {
        String[] options = {"2 zones", "3 zones", "4 zones", "5 zones", "6 zones"};
        
        new AlertDialog.Builder(context)
            .setTitle("Divide into zones")
            .setItems(options, (dialog, which) -> {
                int zoneCount = which + 2; // 2 à 6 zones
                stripCanvas.divideIntoZones(zoneCount);
            })
            .show();
    }
    
    public View getView() {
        return stripView;
    }
    
    public List<Integer> getColors() {
        return stripCanvas.getColors();
    }
    
    public void setColors(List<Integer> colors) {
        stripCanvas.setColors(colors);
    }
    
    public void copyFrom(LedStripEditor otherStrip) {
        setColors(otherStrip.getColors());
    }
    
    public boolean isCopyEnabled() {
        return copyStripCheckbox.isChecked();
    }
    
    public void setOnStripRemovedListener(OnStripRemovedListener listener) {
        this.removeListener = listener;
    }
    
    public void setOnCopyStripListener(OnCopyStripListener listener) {
        this.copyListener = listener;
    }
    
    public void setOnStripChangedListener(LedStripCanvas.OnStripChangedListener listener) {
        stripCanvas.setOnStripChangedListener(listener);
    }
    
    /**
     * Obtenir les valeurs RGBW de toutes les LEDs du strip
     * @return Tableau 2D [ledIndex][r,g,b,w]
     */
    public int[][] getRGBWValues() {
        return stripCanvas.getRGBWValues();
    }
    
    /**
     * Vérifier si ce strip copie le strip 1
     */
    public boolean isCopyingStrip1() {
        return copyStripCheckbox != null && copyStripCheckbox.isChecked();
    }
    
    /**
     * Définir les couleurs de toutes les LEDs du strip
     * @param colors Liste de couleurs RGB (int)
     */
    public void setLedColors(List<Integer> colors) {
        if (stripCanvas != null) {
            stripCanvas.setLedColors(colors);
        }
    }
    
    /**
     * Activer/désactiver le mode copie du strip 1
     * @param copy true pour copier le strip 1
     */
    public void setCopyStrip1(boolean copy) {
        if (copyStripCheckbox != null) {
            copyStripCheckbox.setChecked(copy);
        }
    }
}
