package com.van.management.ui.views;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * Vue personnalisée pour afficher et éditer les couleurs d'un strip LED
 */
public class LedStripCanvas extends View {
    private static final String TAG = "LedStripCanvas";
    
    private int ledCount = 120; // Nombre de LEDs dans le strip (par défaut)
    private List<Integer> ledColors; // Couleur pour chaque LED
    private int selectedColor = Color.WHITE; // Couleur actuellement sélectionnée
    private int brushWidth = 1; // Largeur du crayon en nombre de LEDs
    private Paint ledPaint;
    private Paint zoneBorderPaint; // Lignes de délimitation entre zones seulement
    private Paint outlinePaint; // Contour blanc du strip
    private float ledWidth = 0;
    private float cornerRadius = 8f; // Rayon des coins arrondis
    
    // Zoom et défilement
    private float zoomLevel = 1.0f;
    private float scrollPosition = 0f; // 0.0 à 1.0
    
    // Zones pour les délimitations
    private List<Integer> zoneDelimiters = new java.util.ArrayList<>();
    
    // Interpolation
    public enum InterpolationMode {
        NONE,
        LINEAR
    }
    private InterpolationMode interpolationMode = InterpolationMode.NONE;
    
    // Listener pour les changements
    public interface OnStripChangedListener {
        void onColorsChanged(List<Integer> colors);
    }
    private OnStripChangedListener listener;
    
    public LedStripCanvas(Context context) {
        super(context);
        init();
    }
    
    public LedStripCanvas(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    public LedStripCanvas(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }
    
    private void init() {
        ledColors = new ArrayList<>();
        for (int i = 0; i < ledCount; i++) {
            ledColors.add(Color.BLACK); // Toutes les LEDs commencent éteintes (noir)
        }
        
        ledPaint = new Paint();
        ledPaint.setStyle(Paint.Style.FILL);
        ledPaint.setAntiAlias(true);
        
        zoneBorderPaint = new Paint();
        zoneBorderPaint.setStyle(Paint.Style.STROKE);
        zoneBorderPaint.setColor(Color.WHITE);
        zoneBorderPaint.setStrokeWidth(3);
        zoneBorderPaint.setAntiAlias(true);
        
        outlinePaint = new Paint();
        outlinePaint.setStyle(Paint.Style.STROKE);
        outlinePaint.setColor(Color.WHITE);
        outlinePaint.setStrokeWidth(4);
        outlinePaint.setAntiAlias(true);
        
        // Convertir dp en pixels pour le corner radius
        cornerRadius = 8 * getContext().getResources().getDisplayMetrics().density;
    }
    
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        updateLedWidth();
    }
    
    private void updateLedWidth() {
        ledWidth = (float) getWidth() / ledCount * zoomLevel;
    }
    
    private float calculateOffsetX() {
        if (zoomLevel <= 1.0f) {
            return 0f;
        }
        float maxOffset = getWidth() * (zoomLevel - 1.0f);
        return -scrollPosition * maxOffset;
    }
    
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        
        int width = getWidth();
        int height = getHeight();
        
        // Mettre à jour la largeur des LEDs en fonction du zoom
        updateLedWidth();
        
        // Calculer le clipping en fonction du contenu visible
        float offsetX = calculateOffsetX();
        float totalWidth = ledCount * ledWidth;
        boolean hasContentLeft = (offsetX < 0);
        boolean hasContentRight = (offsetX + totalWidth > width);
        
        // Créer un clip path adaptatif
        canvas.save();
        
        if (!hasContentLeft && !hasContentRight) {
            // Tout visible : clipper avec coins arrondis des deux côtés
            RectF clipRect = new RectF(2, 2, width - 2, height - 2);
            Path clipPath = new Path();
            clipPath.addRoundRect(clipRect, cornerRadius, cornerRadius, Path.Direction.CW);
            canvas.clipPath(clipPath);
        } else if (hasContentLeft && !hasContentRight) {
            // Contenu caché à gauche : clipper seulement le coin droit arrondi
            Path clipPath = new Path();
            clipPath.moveTo(0, 0);
            clipPath.lineTo(width - 2 - cornerRadius, 0);
            clipPath.arcTo(new RectF(width - 2 - cornerRadius * 2, 2, width - 2, 2 + cornerRadius * 2), -90, 90, false);
            clipPath.lineTo(width - 2, height - 2 - cornerRadius);
            clipPath.arcTo(new RectF(width - 2 - cornerRadius * 2, height - 2 - cornerRadius * 2, width - 2, height - 2), 0, 90, false);
            clipPath.lineTo(0, height);
            clipPath.lineTo(0, 0);
            clipPath.close();
            canvas.clipPath(clipPath);
        } else if (!hasContentLeft && hasContentRight) {
            // Contenu caché à droite : clipper seulement le coin gauche arrondi
            Path clipPath = new Path();
            clipPath.moveTo(width, 0);
            clipPath.lineTo(2 + cornerRadius, 0);
            clipPath.arcTo(new RectF(2, 2, 2 + cornerRadius * 2, 2 + cornerRadius * 2), -90, -90, false);
            clipPath.lineTo(2, height - 2 - cornerRadius);
            clipPath.arcTo(new RectF(2, height - 2 - cornerRadius * 2, 2 + cornerRadius * 2, height - 2), 180, -90, false);
            clipPath.lineTo(width, height);
            clipPath.lineTo(width, 0);
            clipPath.close();
            canvas.clipPath(clipPath);
        } else {
            // Contenu caché des deux côtés : clipper simple rectangle (pas de coins arrondis)
            canvas.clipRect(0, 2, width, height - 2);
        }
        
        // Appliquer le zoom et le défilement
        canvas.translate(offsetX, 0);
        
        // Dessiner chaque LED sans bordure (fluide)
        for (int i = 0; i < ledCount; i++) {
            float left = i * ledWidth;
            float right = left + ledWidth;
            
            RectF ledRect = new RectF(left, 0, right, height);
            
            // Dessiner le fond de la LED
            ledPaint.setColor(ledColors.get(i));
            canvas.drawRect(ledRect, ledPaint);
        }
        
        // Dessiner les lignes de délimitation des zones uniquement
        for (int delimiter : zoneDelimiters) {
            if (delimiter > 0 && delimiter < ledCount) {
                float x = delimiter * ledWidth;
                canvas.drawLine(x, 0, x, height, zoneBorderPaint);
            }
        }
        
        canvas.restore();
        
        // Dessiner l'outline adaptative selon le contenu visible
        // (hasContentLeft et hasContentRight déjà calculés plus haut)
        
        Path outlinePath = new Path();
        float left = 2;
        float top = 2;
        float right = width - 2;
        float bottom = height - 2;
        
        if (!hasContentLeft && !hasContentRight) {
            // Tout le contenu est visible : rectangle complet avec coins arrondis
            RectF stripRect = new RectF(left, top, right, bottom);
            canvas.drawRoundRect(stripRect, cornerRadius, cornerRadius, outlinePaint);
        } else {
            // Du contenu est caché : dessiner des segments selon la situation
            
            if (hasContentLeft && !hasContentRight) {
                // Contenu caché à gauche : pas de ligne verticale gauche, coin droit arrondi
                // Ligne horizontale haut
                canvas.drawLine(left - 5, top, right - cornerRadius, top, outlinePaint);
                // Arc coin supérieur droit
                outlinePath.moveTo(right - cornerRadius, top);
                outlinePath.arcTo(new RectF(right - cornerRadius * 2, top, right, top + cornerRadius * 2), -90, 90, false);
                canvas.drawPath(outlinePath, outlinePaint);
                // Ligne verticale droite
                canvas.drawLine(right, top + cornerRadius, right, bottom - cornerRadius, outlinePaint);
                // Arc coin inférieur droit
                outlinePath.reset();
                outlinePath.moveTo(right, bottom - cornerRadius);
                outlinePath.arcTo(new RectF(right - cornerRadius * 2, bottom - cornerRadius * 2, right, bottom), 0, 90, false);
                canvas.drawPath(outlinePath, outlinePaint);
                // Ligne horizontale bas
                canvas.drawLine(right - cornerRadius, bottom, left - 5, bottom, outlinePaint);
                
            } else if (!hasContentLeft && hasContentRight) {
                // Contenu caché à droite : coin gauche arrondi, pas de ligne verticale droite
                // Ligne horizontale haut
                canvas.drawLine(right + 5, top, left + cornerRadius, top, outlinePaint);
                // Arc coin supérieur gauche
                outlinePath.moveTo(left + cornerRadius, top);
                outlinePath.arcTo(new RectF(left, top, left + cornerRadius * 2, top + cornerRadius * 2), -90, -90, false);
                canvas.drawPath(outlinePath, outlinePaint);
                // Ligne verticale gauche
                canvas.drawLine(left, top + cornerRadius, left, bottom - cornerRadius, outlinePaint);
                // Arc coin inférieur gauche
                outlinePath.reset();
                outlinePath.moveTo(left, bottom - cornerRadius);
                outlinePath.arcTo(new RectF(left, bottom - cornerRadius * 2, left + cornerRadius * 2, bottom), 180, -90, false);
                canvas.drawPath(outlinePath, outlinePaint);
                // Ligne horizontale bas
                canvas.drawLine(left + cornerRadius, bottom, right + 5, bottom, outlinePaint);
                
            } else {
                // Contenu caché des deux côtés : juste les lignes horizontales haut et bas
                canvas.drawLine(left, top, right, top, outlinePaint);
                canvas.drawLine(left, bottom, right, bottom, outlinePaint);
            }
        }
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN || 
            event.getAction() == MotionEvent.ACTION_MOVE) {
            
            // Ajuster les coordonnées en fonction du zoom et du défilement
            float offsetX = calculateOffsetX();
            float x = event.getX() - offsetX;
            int centerLedIndex = (int) (x / ledWidth);
            
            // Calculer les indices de début et fin en fonction de brushWidth
            int halfWidth = brushWidth / 2;
            int startIndex = centerLedIndex - halfWidth;
            int endIndex = centerLedIndex + halfWidth;
            
            // Si brushWidth est impair, on ajoute un LED supplémentaire à droite
            if (brushWidth % 2 == 1) {
                endIndex++;
            }
            
            // Appliquer la couleur à toutes les LEDs dans la plage
            boolean changed = false;
            for (int i = startIndex; i < endIndex; i++) {
                if (i >= 0 && i < ledCount) {
                    ledColors.set(i, selectedColor);
                    changed = true;
                }
            }
            
            if (changed) {
                invalidate();
                notifyListener();
                return true;
            }
        }
        return super.onTouchEvent(event);
    }
    
    public void setLedCount(int count) {
        this.ledCount = count;
        ledColors.clear();
        for (int i = 0; i < ledCount; i++) {
            ledColors.add(Color.BLACK);
        }
        requestLayout();
        invalidate();
    }
    
    public void setSelectedColor(int color) {
        this.selectedColor = color;
    }
    
    public int getSelectedColor() {
        return selectedColor;
    }
    
    public void setBrushWidth(int width) {
        this.brushWidth = Math.max(1, width);
    }
    
    public int getBrushWidth() {
        return brushWidth;
    }
    
    public void setZoomLevel(float zoom) {
        this.zoomLevel = Math.max(1.0f, Math.min(zoom, 10.0f));
        if (zoomLevel == 1.0f) {
            scrollPosition = 0f;
        }
        invalidate();
    }
    
    public float getZoomLevel() {
        return zoomLevel;
    }
    
    public void setScrollPosition(float position) {
        this.scrollPosition = Math.max(0f, Math.min(position, 1.0f));
        invalidate();
    }
    
    public float getScrollPosition() {
        return scrollPosition;
    }
    
    public void setInterpolationMode(InterpolationMode mode) {
        this.interpolationMode = mode;
        applyInterpolation();
    }
    
    public InterpolationMode getInterpolationMode() {
        return interpolationMode;
    }
    
    public void reset() {
        for (int i = 0; i < ledCount; i++) {
            ledColors.set(i, Color.BLACK);
        }
        zoneDelimiters.clear();
        invalidate();
        notifyListener();
    }
    
    public void divideIntoZones(int zoneCount) {
        if (zoneCount <= 0 || zoneCount > ledCount) return;
        
        int ledsPerZone = ledCount / zoneCount;
        
        // Palette de couleurs pour les zones
        int[] zoneColors = generateDistinctColors(zoneCount);
        
        // Effacer les anciennes délimitations
        zoneDelimiters.clear();
        
        for (int zone = 0; zone < zoneCount; zone++) {
            int startLed = zone * ledsPerZone;
            int endLed = (zone == zoneCount - 1) ? ledCount : (zone + 1) * ledsPerZone;
            
            for (int i = startLed; i < endLed; i++) {
                ledColors.set(i, zoneColors[zone]);
            }
            
            // Ajouter une ligne de délimitation à la fin de chaque zone (sauf la dernière)
            if (zone < zoneCount - 1) {
                zoneDelimiters.add(endLed);
            }
        }
        
        invalidate();
        notifyListener();
    }
    
    private int[] generateDistinctColors(int count) {
        int[] colors = new int[count];
        for (int i = 0; i < count; i++) {
            float hue = (360f / count) * i;
            colors[i] = Color.HSVToColor(new float[]{hue, 1.0f, 1.0f});
        }
        return colors;
    }
    
    public void applyGradient() {
        // Trouver les zones avec des couleurs différentes
        List<Integer> keyIndexes = new ArrayList<>();
        List<Integer> keyColors = new ArrayList<>();
        
        // Toujours commencer par la première LED
        keyIndexes.add(0);
        keyColors.add(ledColors.get(0));
        
        // Trouver les changements de couleur
        for (int i = 1; i < ledCount; i++) {
            int currentColor = ledColors.get(i);
            int previousColor = ledColors.get(i - 1);
            
            if (currentColor != previousColor) {
                keyIndexes.add(i);
                keyColors.add(currentColor);
            }
        }
        
        if (keyIndexes.size() < 2) return;
        
        // Interpoler entre chaque paire de couleurs clés
        for (int i = 0; i < keyIndexes.size() - 1; i++) {
            int startIndex = keyIndexes.get(i);
            int endIndex = keyIndexes.get(i + 1);
            int startColor = keyColors.get(i);
            int endColor = keyColors.get(i + 1);
            
            interpolateBetween(startIndex, endIndex, startColor, endColor);
        }
        
        invalidate();
        notifyListener();
    }
    
    private void applyInterpolation() {
        if (interpolationMode == InterpolationMode.NONE) return;
        applyGradient();
    }
    
    private void interpolateBetween(int startIndex, int endIndex, int startColor, int endColor) {
        int steps = endIndex - startIndex;
        if (steps <= 1) return;
        
        float startR = Color.red(startColor);
        float startG = Color.green(startColor);
        float startB = Color.blue(startColor);
        
        float endR = Color.red(endColor);
        float endG = Color.green(endColor);
        float endB = Color.blue(endColor);
        
        for (int i = 1; i < steps; i++) {
            float fraction = (float) i / steps;
            int r = (int) (startR + (endR - startR) * fraction);
            int g = (int) (startG + (endG - startG) * fraction);
            int b = (int) (startB + (endB - startB) * fraction);
            
            ledColors.set(startIndex + i, Color.rgb(r, g, b));
        }
    }
    
    public List<Integer> getColors() {
        return new ArrayList<>(ledColors);
    }
    
    public void setColors(List<Integer> colors) {
        if (colors != null && colors.size() == ledCount) {
            ledColors = new ArrayList<>(colors);
            invalidate();
        }
    }
    
    /**
     * Obtenir les valeurs RGBW pour toutes les LEDs
     * Retourne un tableau de tableaux [r, g, b, w] pour chaque LED
     */
    public int[][] getRGBWValues() {
        int[][] rgbwValues = new int[ledCount][4];
        
        for (int i = 0; i < ledCount; i++) {
            int color = ledColors.get(i);
            int r = Color.red(color);
            int g = Color.green(color);
            int b = Color.blue(color);
            
            // Déterminer si c'est du blanc 4000K ou une couleur RGB
            // Si c'est proche du blanc (RGB similaires et élevés), utiliser le canal W
            int minRGB = Math.min(r, Math.min(g, b));
            
            if (minRGB > 200 && Math.abs(r - 255) < 20 && Math.abs(g - 234) < 30 && Math.abs(b - 214) < 40) {
                // C'est du blanc 4000K
                rgbwValues[i][0] = 0;
                rgbwValues[i][1] = 0;
                rgbwValues[i][2] = 0;
                rgbwValues[i][3] = minRGB; // Utiliser la valeur minimale comme W
            } else {
                // C'est une couleur RGB (ou noir)
                rgbwValues[i][0] = r;
                rgbwValues[i][1] = g;
                rgbwValues[i][2] = b;
                rgbwValues[i][3] = 0;
            }
        }
        
        return rgbwValues;
    }
    
    public void setOnStripChangedListener(OnStripChangedListener listener) {
        this.listener = listener;
    }
    
    private void notifyListener() {
        if (listener != null) {
            listener.onColorsChanged(getColors());
        }
    }
    
    /**
     * Définir les couleurs de toutes les LEDs
     * @param colors Liste de couleurs RGB (int)
     */
    public void setLedColors(List<Integer> colors) {
        if (colors != null && colors.size() >= ledCount) {
            for (int i = 0; i < ledCount; i++) {
                ledColors.set(i, colors.get(i));
            }
            invalidate(); // Redessiner le canvas
            notifyListener();
        }
    }
}
