package com.van.management.ui.views;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.Nullable;

/**
 * Slider vertical personnalisé pour le canal White avec gradient de noir à 4000K
 */
public class VerticalWhiteSlider extends View {
    // Constante pour la couleur White 4000K - MODIFIEZ ICI pour changer la teinte
    private static final int WHITE_4000K_R = 255;
    private static final int WHITE_4000K_G = 234;
    private static final int WHITE_4000K_B = 224;
    
    private Paint gradientPaint;
    private Paint linePaint;
    private Paint lineOutlinePaint;
    private int whiteValue = 0; // 0-255
    private float lineY = 0; // Position Y de la ligne
    private RectF sliderRect = new RectF();
    
    public interface OnWhiteValueChangeListener {
        void onWhiteValueChanged(int value);
    }
    
    private OnWhiteValueChangeListener listener;
    
    public VerticalWhiteSlider(Context context) {
        super(context);
        init();
    }
    
    public VerticalWhiteSlider(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    public VerticalWhiteSlider(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }
    
    private void init() {
        gradientPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        
        linePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        linePaint.setStyle(Paint.Style.STROKE);
        linePaint.setColor(Color.WHITE);
        linePaint.setStrokeWidth(4);
        
        lineOutlinePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        lineOutlinePaint.setStyle(Paint.Style.STROKE);
        lineOutlinePaint.setColor(Color.BLACK);
        lineOutlinePaint.setStrokeWidth(6);
    }
    
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        
        // Définir le rectangle du slider avec une petite marge
        float margin = 10;
        sliderRect.set(margin, margin, w - margin, h - margin);
        
        // Créer le gradient de blanc naturel 4000K (en haut) à noir (en bas)
        LinearGradient gradient = new LinearGradient(
            0, sliderRect.top,
            0, sliderRect.bottom,
            Color.rgb(WHITE_4000K_R, WHITE_4000K_G, WHITE_4000K_B), // Blanc naturel 4000K en haut
            Color.BLACK, // Noir en bas
            Shader.TileMode.CLAMP
        );
        gradientPaint.setShader(gradient);
        
        // Initialiser la position de la ligne
        updateLinePosition();
    }
    
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        
        // Dessiner le rectangle avec le gradient
        float cornerRadius = 8;
        canvas.drawRoundRect(sliderRect, cornerRadius, cornerRadius, gradientPaint);
        
        // Dessiner la ligne de sélection avec contour noir
        float lineLeft = sliderRect.left;
        float lineRight = sliderRect.right;
        
        // Contour noir
        canvas.drawLine(lineLeft - 5, lineY, lineRight + 5, lineY, lineOutlinePaint);
        
        // Ligne blanche
        canvas.drawLine(lineLeft - 5, lineY, lineRight + 5, lineY, linePaint);
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN || 
            event.getAction() == MotionEvent.ACTION_MOVE) {
            
            float y = event.getY();
            
            // Limiter Y dans les bornes du slider
            if (y < sliderRect.top) y = sliderRect.top;
            if (y > sliderRect.bottom) y = sliderRect.bottom;
            
            lineY = y;
            
            // Calculer la valeur du white (inversé: en haut = 255, en bas = 0)
            float percent = (y - sliderRect.top) / (sliderRect.bottom - sliderRect.top);
            whiteValue = Math.round((1 - percent) * 255);
            
            invalidate();
            
            if (listener != null) {
                listener.onWhiteValueChanged(whiteValue);
            }
            
            return true;
        }
        
        return super.onTouchEvent(event);
    }
    
    public void setWhiteValue(int value) {
        if (value < 0) value = 0;
        if (value > 255) value = 255;
        
        whiteValue = value;
        updateLinePosition();
        invalidate();
    }
    
    public int getWhiteValue() {
        return whiteValue;
    }
    
    private void updateLinePosition() {
        if (sliderRect.height() > 0) {
            // Calculer Y depuis la valeur (inversé: 255 en haut, 0 en bas)
            float percent = 1 - (whiteValue / 255f);
            lineY = sliderRect.top + percent * (sliderRect.bottom - sliderRect.top);
        }
    }
    
    public void setOnWhiteValueChangeListener(OnWhiteValueChangeListener listener) {
        this.listener = listener;
    }
}
