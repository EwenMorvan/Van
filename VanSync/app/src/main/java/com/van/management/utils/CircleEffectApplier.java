package com.van.management.utils;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.graphics.drawable.Drawable;
import android.renderscript.Allocation;
import android.renderscript.Element;
import android.renderscript.RenderScript;
import android.renderscript.ScriptIntrinsicBlur;

import java.util.List;

public class CircleEffectApplier extends Drawable {

    private final Paint paintCircle = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final List<Integer> colors;
    private final RenderScript renderScript;
    private Bitmap cachedBitmap;
    private int lastWidth = -1;
    private int lastHeight = -1;

    public CircleEffectApplier(RenderScript rs, List<Integer> colors) {
        this.renderScript = rs;
        this.colors = colors;
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();
        int width = bounds.width();
        int height = bounds.height();

        if (width <= 0 || height <= 0) return;

        // Recréer le bitmap si les dimensions changent
        if (cachedBitmap == null || lastWidth != width || lastHeight != height) {
            if (cachedBitmap != null) {
                cachedBitmap.recycle();
            }
            cachedBitmap = createBlurredCircle(width, height);
            lastWidth = width;
            lastHeight = height;
        }

        if (cachedBitmap != null) {
            canvas.drawBitmap(cachedBitmap, bounds.left, bounds.top, null);
        }
    }

    private Bitmap createBlurredCircle(int width, int height) {
        // Ajouter du padding pour le halo du blur
        int blurRadius = 15; // Blur plus fort (max RenderScript = 25)
        int haloPadding = blurRadius; // Padding pour le halo
        
        // Créer un bitmap pour le cercle avec son halo
        Bitmap originalBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas originalCanvas = new Canvas(originalBitmap);

        float cx = width / 2f;
        float cy = height / 2f;
        // Réduire le rayon du cercle pour laisser de la place au halo
        float radius = Math.min(width, height) / 2f - haloPadding;

        // -----------------------------
        // 1) Dessiner le gradient angulaire
        // -----------------------------
        int n = colors.size();
        int[] colorArray = new int[n + 1];
        float[] positions = new float[n + 1];

        for (int i = 0; i < n; i++) {
            colorArray[i] = colors.get(i);
            positions[i] = (float) i / (float) n;
        }

        colorArray[n] = colors.get(0);
        positions[n] = 1f;

        Shader angular = new SweepGradient(cx, cy, colorArray, positions);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setShader(angular);

        originalCanvas.drawCircle(cx, cy, radius, paint);

        // -----------------------------
        // 2) Appliquer le blur avec RenderScript
        // -----------------------------
        Bitmap blurredBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        
        try {
            Allocation input = Allocation.createFromBitmap(renderScript, originalBitmap);
            Allocation output = Allocation.createFromBitmap(renderScript, blurredBitmap);

            ScriptIntrinsicBlur script = ScriptIntrinsicBlur.create(renderScript, Element.U8_4(renderScript));
            script.setRadius(blurRadius); // Blur fort (25 = max)
            script.setInput(input);
            script.forEach(output);
            output.copyTo(blurredBitmap);

            input.destroy();
            output.destroy();
            script.destroy();
        } catch (Exception e) {
            e.printStackTrace();
            // En cas d'erreur, retourner l'original
            originalBitmap.recycle();
            return Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        }

        // Le bitmap blurré contient déjà le cercle avec son halo
        // Le halo s'étend naturellement dans l'espace réservé
        originalBitmap.recycle();

        return blurredBitmap;
    }

    public void cleanup() {
        if (cachedBitmap != null) {
            cachedBitmap.recycle();
            cachedBitmap = null;
        }
    }

    @Override public void setAlpha(int alpha) {}
    @Override public void setColorFilter(android.graphics.ColorFilter cf) {}
    @Override public int getOpacity() { return android.graphics.PixelFormat.TRANSLUCENT; }
}
