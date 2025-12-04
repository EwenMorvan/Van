package com.van.management.ui.battery;



import android.widget.TextView;
import com.van.management.R;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.*;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;
import androidx.annotation.Nullable;
import androidx.constraintlayout.widget.ConstraintLayout;

public class BatteryGaugeView extends ConstraintLayout {
    private Paint arcPaint;
    private RectF arcRect;
    private int batteryLevel = 0;
    private int gaugeColor;
    private float gaugeWidth;
    private float bgStrokeWidth;

    public BatteryGaugeView(Context context) {
        super(context);
        init(context, null);
    }

    public BatteryGaugeView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init(context, attrs);
    }

    public BatteryGaugeView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context, attrs);
    }

    private void init(Context context, AttributeSet attrs) {
        // Récupérer la largeur du stroke du background
        bgStrokeWidth = getBackgroundStrokeWidth(context);

        // Configurer l'épaisseur de l'arc (plus large que le bg pour bien recouvrir)
        gaugeWidth = bgStrokeWidth;

        // Configuration du paint pour l'arc
        arcPaint = new Paint();
        arcPaint.setStyle(Paint.Style.STROKE);
        arcPaint.setStrokeCap(Paint.Cap.ROUND); // Bouts arrondis
        arcPaint.setAntiAlias(true);

        // Couleur de l'arc (vert par défaut)
        gaugeColor = Color.parseColor("#4CAF50");
        arcPaint.setColor(gaugeColor);
        arcPaint.setStrokeWidth(gaugeWidth);

        // Rectangle pour l'arc
        arcRect = new RectF();

        // Important pour que onDraw soit appelé
        setWillNotDraw(false);
    }

    private float getBackgroundStrokeWidth(Context context) {
        try {
            // Récupérer la dimension depuis les resources
            int strokeWidthResId = getResources().getIdentifier(
                    "battery_gauge_bg_stroke", "dimen", context.getPackageName());

            if (strokeWidthResId != 0) {
                return getResources().getDimension(strokeWidthResId);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        // Fallback: 8dp
        return dpToPx(8, context);
    }

    private float dpToPx(float dp, Context context) {
        return TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP,
                dp,
                context.getResources().getDisplayMetrics()
        );
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        // POUR RECOUVRIR LE BACKGROUND :
        // Le stroke du bg est dessiné À L'EXTÉRIEUR
        // Donc pour que l'arc recouvre le bg, on doit le dessiner AU-DESSUS du stroke du bg
        float margin = gaugeWidth / 2;

        arcRect.set(margin, margin, w - margin, h - margin);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (batteryLevel > 0) {
            // Calculer l'angle de l'arc (0% = 0°, 100% = 360°)
            float sweepAngle = (batteryLevel * 360f) / 100f;

            // Dessiner l'arc
            // startAngle: -90 pour commencer à 12h (0° = 3h, -90° = 12h)
            canvas.drawArc(arcRect, -90, sweepAngle, false, arcPaint);
        }
    }

    // Méthode pour mettre à jour le niveau de batterie
    public void setBatteryLevel(int level) {
        this.batteryLevel = Math.max(0, Math.min(100, level));
        updateGaugeColor();
        invalidate();
        updateBatteryLevelText();
    }

    public int getBatteryLevel() {
        return batteryLevel;
    }

    // Méthode pour changer la couleur de l'arc
    public void setGaugeColor(int color) {
        this.gaugeColor = color;
        arcPaint.setColor(color);
        invalidate();
    }

    public void setGaugeWidth(float widthDp) {
        this.gaugeWidth = dpToPx(widthDp, getContext());
        arcPaint.setStrokeWidth(gaugeWidth);
        requestLayout(); // Forcer le recalcul des dimensions
        invalidate();
    }

    private void updateGaugeColor() {
        // Transition continue du rouge au vert sur toute la plage 0-100%
        float ratio = batteryLevel / 100f;

        int color = interpolateColor(
                Color.parseColor("#FF5252"), // Rouge à 0%
                Color.parseColor("#4CAF50"), // Vert à 100%
                ratio
        );

        arcPaint.setColor(color);
    }

    private int interpolateColor(int startColor, int endColor, float ratio) {
        ratio = Math.max(0, Math.min(1, ratio));

        int startA = (startColor >> 24) & 0xff;
        int startR = (startColor >> 16) & 0xff;
        int startG = (startColor >> 8) & 0xff;
        int startB = startColor & 0xff;

        int endA = (endColor >> 24) & 0xff;
        int endR = (endColor >> 16) & 0xff;
        int endG = (endColor >> 8) & 0xff;
        int endB = endColor & 0xff;

        return ((startA + (int)((endA - startA) * ratio)) << 24) |
                ((startR + (int)((endR - startR) * ratio)) << 16) |
                ((startG + (int)((endG - startG) * ratio)) << 8) |
                ((startB + (int)((endB - startB) * ratio)));
    }

    private void updateBatteryLevelText() {
        View textView = findViewById(R.id.battery_gauge_level);
        if (textView instanceof TextView) {
            ((TextView) textView).setText(batteryLevel + "%");
        }
    }
}