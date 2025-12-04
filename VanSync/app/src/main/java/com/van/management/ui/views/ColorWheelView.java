package com.van.management.ui.views;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PointF;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.Nullable;

/**
 * Vue personnalisée pour afficher une roue de couleur (color wheel)
 * Centre blanc, couleurs saturées sur le bord, transition fluide
 */
public class ColorWheelView extends View {
    private Paint wheelPaint;
    private Paint cursorPaint;
    private Paint cursorBorderPaint;
    private int currentColor = Color.WHITE;
    private PointF cursorPosition = new PointF(0.5f, 0.5f); // Position normalisée (0-1)
    private float radius = 0;
    private PointF center = new PointF();
    
    public interface OnColorChangedListener {
        void onColorChanged(int color);
    }
    
    private OnColorChangedListener listener;
    
    public ColorWheelView(Context context) {
        super(context);
        init();
    }
    
    public ColorWheelView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    public ColorWheelView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }
    
    private void init() {
        wheelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        
        cursorPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        cursorPaint.setStyle(Paint.Style.FILL);
        cursorPaint.setColor(Color.WHITE);
        
        cursorBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        cursorBorderPaint.setStyle(Paint.Style.STROKE);
        cursorBorderPaint.setColor(Color.BLACK);
        cursorBorderPaint.setStrokeWidth(3);
    }
    
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        
        int size = Math.min(w, h);
        radius = size / 2f - 20; // Marge de 20px
        center.set(w / 2f, h / 2f);
        
        // Créer la roue de couleur
        createColorWheel();
    }
    
    private Bitmap colorWheelBitmap;
    
    private void createColorWheel() {
        if (radius <= 0) return;
        
        int size = (int) (radius * 2);
        colorWheelBitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(colorWheelBitmap);
        
        float centerX = size / 2f;
        float centerY = size / 2f;
        
        // Dessiner la roue de couleur pixel par pixel pour une transition fluide
        for (int x = 0; x < size; x++) {
            for (int y = 0; y < size; y++) {
                float dx = x - centerX;
                float dy = y - centerY;
                float distance = (float) Math.sqrt(dx * dx + dy * dy);
                
                if (distance <= radius) {
                    // Calculer l'angle pour la teinte (hue)
                    float angle = (float) Math.toDegrees(Math.atan2(dy, dx));
                    if (angle < 0) angle += 360;
                    
                    // Calculer la saturation basée sur la distance du centre
                    float saturation = distance / radius;
                    
                    // HSV: Hue = angle, Saturation = distance/radius, Value = 1.0
                    int color = Color.HSVToColor(new float[]{angle, saturation, 1.0f});
                    colorWheelBitmap.setPixel(x, y, color);
                }
            }
        }
    }
    
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        
        if (colorWheelBitmap != null) {
            // Dessiner la roue de couleur
            float left = center.x - radius;
            float top = center.y - radius;
            canvas.drawBitmap(colorWheelBitmap, left, top, null);
        }
        
        // Dessiner le curseur à la position actuelle
        float cursorX = center.x + (cursorPosition.x - 0.5f) * 2 * radius;
        float cursorY = center.y + (cursorPosition.y - 0.5f) * 2 * radius;
        
        canvas.drawCircle(cursorX, cursorY, 12, cursorPaint);
        canvas.drawCircle(cursorX, cursorY, 12, cursorBorderPaint);
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN || 
            event.getAction() == MotionEvent.ACTION_MOVE) {
            
            float x = event.getX();
            float y = event.getY();
            
            // Convertir en coordonnées relatives au centre
            float dx = x - center.x;
            float dy = y - center.y;
            float distance = (float) Math.sqrt(dx * dx + dy * dy);
            
            // Limiter à l'intérieur du cercle
            if (distance > radius) {
                dx = dx / distance * radius;
                dy = dy / distance * radius;
                distance = radius;
            }
            
            // Mettre à jour la position du curseur (normalisée 0-1)
            cursorPosition.x = 0.5f + dx / (2 * radius);
            cursorPosition.y = 0.5f + dy / (2 * radius);
            
            // Calculer la couleur à cette position
            float angle = (float) Math.toDegrees(Math.atan2(dy, dx));
            if (angle < 0) angle += 360;
            
            float saturation = distance / radius;
            
            currentColor = Color.HSVToColor(new float[]{angle, saturation, 1.0f});
            
            invalidate();
            
            if (listener != null) {
                listener.onColorChanged(currentColor);
            }
            
            return true;
        }
        
        return super.onTouchEvent(event);
    }
    
    public void setColor(int color) {
        currentColor = color;
        
        // Convertir RGB en HSV pour positionner le curseur
        float[] hsv = new float[3];
        Color.colorToHSV(color, hsv);
        
        float hue = hsv[0]; // 0-360
        float saturation = hsv[1]; // 0-1
        
        // Calculer la position du curseur
        float angle = (float) Math.toRadians(hue);
        float distance = saturation * radius;
        
        float dx = (float) Math.cos(angle) * distance;
        float dy = (float) Math.sin(angle) * distance;
        
        cursorPosition.x = 0.5f + dx / (2 * radius);
        cursorPosition.y = 0.5f + dy / (2 * radius);
        
        invalidate();
    }
    
    public int getColor() {
        return currentColor;
    }
    
    public void setOnColorChangedListener(OnColorChangedListener listener) {
        this.listener = listener;
    }
}
