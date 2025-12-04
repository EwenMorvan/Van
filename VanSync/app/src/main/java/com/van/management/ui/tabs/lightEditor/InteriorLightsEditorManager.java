package com.van.management.ui.tabs.lightEditor;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.van.management.R;

import java.util.ArrayList;
import java.util.List;

public class InteriorLightsEditorManager {

    public static class LightMode {
        public String name;
        public List<Integer> colors;
        public View buttonView;
        
        public LightMode(String name, List<Integer> colors) {
            this.name = name;
            this.colors = colors;
        }
    }

    private android.content.Context context;
    private LinearLayout leftColumn;
    private LinearLayout rightColumn;
    private List<LightMode> lightModes = new ArrayList<>();
    private android.renderscript.RenderScript renderScript;

    public InteriorLightsEditorManager(android.content.Context context, View rootView, android.renderscript.RenderScript renderScript) {
        this.context = context;
        this.renderScript = renderScript;
        this.leftColumn = rootView.findViewById(R.id.left_column);
        this.rightColumn = rootView.findViewById(R.id.right_column);
    }

    // Interface pour gérer les clics sur les boutons
    public interface OnButtonClickListener {
        void onButtonClicked(View buttonView, String buttonName);
    }
    
    // Interface pour gérer le drag-and-drop
    public interface OnButtonSwapListener {
        void onButtonsSwapped(int fromIndex, int toIndex);
    }
    
    private OnButtonClickListener clickListener;
    private OnButtonSwapListener swapListener;
    
    // Méthode pour définir le listener de clic
    public void setOnButtonClickListener(OnButtonClickListener listener) {
        this.clickListener = listener;
    }
    
    // Méthode pour définir le listener de swap
    public void setOnButtonSwapListener(OnButtonSwapListener listener) {
        this.swapListener = listener;
    }
    
    // Méthode pour ajouter un bouton dynamiquement
    public View addLightModeButton(String buttonName, List<Integer> colors) {
        LayoutInflater inflater = LayoutInflater.from(context);
        View lightModeButton = inflater.inflate(R.layout.light_mode_button, null);
        
        // Créer l'objet LightMode
        LightMode mode = new LightMode(buttonName, colors);
        mode.buttonView = lightModeButton;
        lightModes.add(mode);

        // Mettre à jour l'affichage du bouton
        updateButtonDisplay(lightModeButton, lightModes.size(), buttonName, colors);

        // Configurer les paramètres de layout
        LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                context.getResources().getDimensionPixelSize(R.dimen.card_button_large)
        );
        int marginTop = context.getResources().getDimensionPixelSize(R.dimen.margin_xlarge);
        layoutParams.setMargins(0, marginTop, 0, 0);
        lightModeButton.setLayoutParams(layoutParams);

        // Récupérer le container du bouton pour gérer la sélection
        View buttonContainer = lightModeButton.findViewById(R.id.light_mode_button_container);
        
        // Ajouter un écouteur de clic
        if (buttonContainer != null) {
            buttonContainer.setOnClickListener(v -> {
                if (clickListener != null) {
                    clickListener.onButtonClicked(v, buttonName);
                }
            });
            
            // Ajouter un long-click listener pour le drag-and-drop
            buttonContainer.setOnLongClickListener(v -> {
                // Trouver l'index actuel du mode dans la liste
                int modeIndex = -1;
                for (int i = 0; i < lightModes.size(); i++) {
                    View container = lightModes.get(i).buttonView.findViewById(R.id.light_mode_button_container);
                    if (container == v) {
                        modeIndex = i;
                        break;
                    }
                }
                
                // Ne pas permettre de déplacer le premier mode (Off - index 0)
                if (modeIndex == 0) {
                    android.util.Log.d("InteriorLightsEditor", "Cannot drag the 'Off' mode");
                    return false;
                }
                
                if (modeIndex < 0) {
                    return false;
                }
                
                // Créer le DragShadow
                View.DragShadowBuilder shadowBuilder = new View.DragShadowBuilder(lightModeButton);
                
                // Démarrer le drag avec l'index du mode
                v.startDragAndDrop(null, shadowBuilder, modeIndex, 0);
                
                // Changer l'apparence pendant le drag
                lightModeButton.setAlpha(0.5f);
                
                android.util.Log.d("InteriorLightsEditor", "Started dragging mode at index " + modeIndex);
                return true;
            });
            
            // Ajouter un DragListener pour recevoir les drops
            buttonContainer.setOnDragListener((v, event) -> {
                switch (event.getAction()) {
                    case android.view.DragEvent.ACTION_DRAG_STARTED:
                        // Sauvegarder le background original
                        v.setTag(R.id.light_mode_button_container, v.getBackground());
                        return true;
                        
                        
                    case android.view.DragEvent.ACTION_DRAG_ENTERED:
                        // Effet subtil: augmenter légèrement l'élévation et changer l'alpha
                        v.setAlpha(0.7f);
                        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                            v.setElevation(8f);
                        }
                        return true;
                        
                    case android.view.DragEvent.ACTION_DRAG_EXITED:
                        // Restaurer l'apparence normale
                        v.setAlpha(1.0f);
                        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                            v.setElevation(0f);
                        }
                        return true;
                        
                    case android.view.DragEvent.ACTION_DROP:
                        // Récupérer l'index du mode draggé
                        int fromIndex = (Integer) event.getLocalState();
                        
                        // Trouver l'index du mode cible
                        int toIndex = -1;
                        for (int i = 0; i < lightModes.size(); i++) {
                            View container = lightModes.get(i).buttonView.findViewById(R.id.light_mode_button_container);
                            if (container == v) {
                                toIndex = i;
                                break;
                            }
                        }
                        
                        if (toIndex >= 0 && toIndex != fromIndex) {
                            android.util.Log.d("InteriorLightsEditor", "Dropping mode " + fromIndex + " onto " + toIndex);
                            swapModes(fromIndex, toIndex);
                        }
                        
                        // Restaurer l'apparence
                        v.setAlpha(1.0f);
                        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                            v.setElevation(0f);
                        }
                        return true;
                        
                    case android.view.DragEvent.ACTION_DRAG_ENDED:
                        // Restaurer l'alpha et l'élévation de tous les boutons
                        for (LightMode m : lightModes) {
                            View container = m.buttonView.findViewById(R.id.light_mode_button_container);
                            if (container != null) {
                                container.setAlpha(1.0f);
                                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                                    container.setElevation(0f);
                                }
                            }
                            m.buttonView.setAlpha(1.0f);
                        }
                        
                        if (event.getResult()) {
                            android.util.Log.d("InteriorLightsEditor", "Drag successful");
                        }
                        return true;
                        
                    default:
                        return false;
                }
            });
        }

        // Déterminer dans quelle colonne ajouter le bouton
        if (leftColumn.getChildCount() <= rightColumn.getChildCount()) {
            leftColumn.addView(lightModeButton);
        } else {
            rightColumn.addView(lightModeButton);
        }

        return buttonContainer; // Retourner le container pour permettre la sélection
    }
    
    // Méthode pour mettre à jour l'affichage d'un bouton
    private void updateButtonDisplay(View buttonView, int number, String name, List<Integer> colors) {
        TextView numberText = buttonView.findViewById(R.id.light_mode_number_text);
        TextView nameText = buttonView.findViewById(R.id.light_mode_name_text);
        View circleView = buttonView.findViewById(R.id.circleView);
        
        if (numberText != null) {
            numberText.setText(number + ".");
        }
        if (nameText != null) {
            nameText.setText(name);
        }
        if (circleView != null && renderScript != null) {
            circleView.setBackground(new com.van.management.utils.CircleEffectApplier(renderScript, colors));
        }
    }
    
    // Méthode pour supprimer un bouton spécifique
    public boolean removeButton(View buttonContainer) {
        // Trouver le mode correspondant
        LightMode modeToRemove = null;
        int indexToRemove = -1;
        
        for (int i = 0; i < lightModes.size(); i++) {
            View container = lightModes.get(i).buttonView.findViewById(R.id.light_mode_button_container);
            if (container == buttonContainer) {
                modeToRemove = lightModes.get(i);
                indexToRemove = i;
                break;
            }
        }
        
        if (modeToRemove == null || indexToRemove == 0) {
            // Ne pas supprimer le premier mode
            return false;
        }
        
        // Supprimer de la liste
        lightModes.remove(indexToRemove);
        
        // Supprimer de la vue
        if (modeToRemove.buttonView.getParent() instanceof LinearLayout) {
            ((LinearLayout) modeToRemove.buttonView.getParent()).removeView(modeToRemove.buttonView);
        }
        
        // Réorganiser tous les boutons
        reorganizeButtons();
        
        return true;
    }
    
    // Méthode pour réorganiser tous les boutons après suppression
    private void reorganizeButtons() {
        // Vider les colonnes
        leftColumn.removeAllViews();
        rightColumn.removeAllViews();
        
        // Rajouter tous les boutons dans l'ordre avec les bons numéros
        for (int i = 0; i < lightModes.size(); i++) {
            LightMode mode = lightModes.get(i);
            
            // Mettre à jour l'affichage avec le nouveau numéro
            updateButtonDisplay(mode.buttonView, i + 1, mode.name, mode.colors);
            
            // S'assurer que le container a son style original
            View container = mode.buttonView.findViewById(R.id.light_mode_button_container);
            if (container != null) {
                // Restaurer l'alpha et l'élévation
                container.setAlpha(1.0f);
                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                    container.setElevation(0f);
                }
            }
            
            // Restaurer l'alpha du bouton entier
            mode.buttonView.setAlpha(1.0f);
            
            // Ajouter dans la bonne colonne
            if (leftColumn.getChildCount() <= rightColumn.getChildCount()) {
                leftColumn.addView(mode.buttonView);
            } else {
                rightColumn.addView(mode.buttonView);
            }
        }
    }

    // Méthode pour supprimer tous les boutons
    public void removeAllButtons() {
        leftColumn.removeAllViews();
        rightColumn.removeAllViews();
        lightModes.clear();
    }

    // Méthode pour obtenir le nombre de boutons
    public int getButtonCount() {
        return lightModes.size();
    }
    
    // Méthode pour obtenir tous les modes
    public List<LightMode> getLightModes() {
        return new ArrayList<>(lightModes);
    }
    
    /**
     * Échange deux modes dans l'affichage
     * @param fromIndex Index du mode source (dans lightModes)
     * @param toIndex Index du mode destination (dans lightModes)
     */
    public void swapModes(int fromIndex, int toIndex) {
        // Ne pas permettre de déplacer le premier mode (Off)
        if (fromIndex == 0 || toIndex == 0) {
            android.util.Log.w("InteriorLightsEditor", "Cannot move the 'Off' mode");
            return;
        }
        
        if (fromIndex < 0 || fromIndex >= lightModes.size() || 
            toIndex < 0 || toIndex >= lightModes.size()) {
            android.util.Log.w("InteriorLightsEditor", "Invalid swap indices");
            return;
        }
        
        // Échanger dans la liste
        LightMode temp = lightModes.get(fromIndex);
        lightModes.set(fromIndex, lightModes.get(toIndex));
        lightModes.set(toIndex, temp);
        
        // Réorganiser l'affichage
        reorganizeButtons();
        
        // Notifier le listener
        if (swapListener != null) {
            swapListener.onButtonsSwapped(fromIndex, toIndex);
        }
    }

    // Callback pour le clic sur un bouton
    private void onLightModeButtonClicked(String buttonName) {
        // Implémentez votre logique ici
        android.util.Log.d("LightMode", "Bouton cliqué: " + buttonName);
    }
}