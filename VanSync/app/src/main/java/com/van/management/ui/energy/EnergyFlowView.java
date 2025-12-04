package com.van.management.ui.energy;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;

import androidx.core.content.ContextCompat;

import com.van.management.R;

/**
 * Vue personnalisée pour afficher un diagramme de flux d'énergie type Sankey
 * Sources (gauche): Panneau solaire, Alternateur, Secteur/Chargeur
 * Centre: Batterie
 * Destinations (droite): Systèmes 12V, Onduleur 220V
 */
public class EnergyFlowView extends View {
    
    // Peintures
    private Paint flowPaint;
    private Paint flowInnerGlowPaint;
    private Paint flowOuterStrokePaint;
    private Paint iconCirclePaint;
    private Paint iconCircleStrokePaint;
    private Paint iconGlowPaint;
    private Paint textPaint;
    
    // Drawables
    private Drawable batteryIcon;
    private Drawable solarIcon;
    private Drawable sector220vIcon;
    private Drawable chargerExtIcon;
    private Drawable alternatorIcon;
    private Drawable lampIcon;
    
    // Couleurs pour flux entrant (vers batterie)
    private int colorIncomingStart = Color.parseColor("#00D4FF");
    private int colorIncomingEnd = Color.parseColor("#00A8CC");
    private int colorIncomingGlow = Color.parseColor("#00FFFF");
    
    // Couleurs pour flux sortant (depuis batterie)
    private int colorOutgoingStart = Color.parseColor("#FF8C00");
    private int colorOutgoingEnd = Color.parseColor("#CC5500");
    private int colorOutgoingGlow = Color.parseColor("#FFB800");
    
    // Couleurs des icônes
    private int colorSolar;
    private int colorAlternator;
    private int colorCharger;
    private int colorBattery;
    private int color12V;
    private int color220V;
    
    // Puissances maximales pour chaque source/device (en Watts)
    private static final float MAX_POWER_MPPT_100_50 = 520f;
    private static final float MAX_POWER_MPPT_70_15 = 200f;
    private static final float MAX_POWER_ALTERNATOR = 400f;
    private static final float MAX_POWER_CHARGER = 400f;
    private static final float MAX_POWER_12V = 500f;
    private static final float MAX_POWER_220V = 1000f;
    private static final float MAX_POWER_BATTERY = 1000f;
    
    // Données de puissance des devices (en Watts) - mesures directes
    private float solarPower = 0f;
    private float solar1Power = 0f; // MPPT 100/50
    private float solar2Power = 0f; // MPPT 70/15
    private float alternatorPower = 0f;
    private float chargerPower = 0f;
    private float system12vPower = 0f;
    private float system220vPower = 0f;
    private float batteryPower = 0f; // Positif = charge, négatif = décharge
    
    // États du système 220V
    private boolean inverterEnabled = true; // Les prises 220V sont activées
    private boolean is220vPassthrough = false; // Le secteur 220V passe directement aux prises (bypass batterie)
    
    // Flux d'énergie explicites (calculés par EnergyTab basés sur les mesures réelles)
    // Format: [source]To[destination] en Watts
    private float solar1ToBattery = 0f;
    private float solar2ToBattery = 0f;
    private float solar1To12v = 0f;
    private float solar2To12v = 0f;
    private float solar1To220v = 0f;
    private float solar2To220v = 0f;
    private float alternatorToBattery = 0f;
    private float alternatorTo12v = 0f;
    private float alternatorTo220v = 0f;
    private float chargerToBattery = 0f;
    private float chargerTo12v = 0f;
    private float chargerTo220v = 0f;
    private float batteryTo12v = 0f;
    private float batteryTo220v = 0f;
    
    // Positions des éléments
    private float centerX, centerY;
    private float iconRadius = 40f;
    private float spacing = 150f;
    
    // Animation
    private float pulsePhase = 0f; // 0 to 2*PI for sine wave
    private float flowOffset = 0f; // 0 to 1 for flowing energy effect
    private boolean animationEnabled = true;
    private long lastFrameTime = 0;
    
    public EnergyFlowView(Context context) {
        super(context);
        init();
    }
    
    public EnergyFlowView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    private void init() {
        // Initialiser les couleurs des icônes
        colorSolar = Color.parseColor("#FFA726"); // Orange pour solaire
        colorAlternator = Color.parseColor("#7E57C2"); // Bleu pour alternateur
        colorCharger = Color.parseColor("#26A69A"); // Vert pour chargeur
        colorBattery = Color.parseColor("#FFFFFF"); // Blanc pour batterie
        color12V = Color.parseColor("#42A5F5"); // Cyan pour 12V
        color220V = Color.parseColor("#EF5350"); // Rose pour 220V
        
        // Paint pour les flux principaux (ribbon)
        flowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        flowPaint.setStyle(Paint.Style.FILL);
        flowPaint.setStrokeCap(Paint.Cap.ROUND);
        flowPaint.setStrokeJoin(Paint.Join.ROUND);
        
        // Paint pour le glow intérieur (30% opacity, blur 12dp)
        flowInnerGlowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        flowInnerGlowPaint.setStyle(Paint.Style.FILL);
        flowInnerGlowPaint.setMaskFilter(new android.graphics.BlurMaskFilter(12f, android.graphics.BlurMaskFilter.Blur.NORMAL));
        
        // Paint pour le stroke extérieur (15% opacity, 2dp)
        flowOuterStrokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        flowOuterStrokePaint.setStyle(Paint.Style.STROKE);
        flowOuterStrokePaint.setStrokeWidth(2f);
        flowOuterStrokePaint.setStrokeCap(Paint.Cap.ROUND);
        flowOuterStrokePaint.setStrokeJoin(Paint.Join.ROUND);
        
        // Paint pour les cercles d'icônes (transparent avec outline)
        iconCirclePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        iconCirclePaint.setStyle(Paint.Style.FILL);
        iconCirclePaint.setColor(0x30FFFFFF); // Transparent blanc 20%
        
        // Paint pour l'outline des cercles
        iconCircleStrokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        iconCircleStrokePaint.setStyle(Paint.Style.STROKE);
        iconCircleStrokePaint.setStrokeWidth(3f);
        iconCircleStrokePaint.setColor(0x80FFFFFF); // Semi-transparent blanc 50%
        
        // Paint pour le glow des icônes
        iconGlowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        iconGlowPaint.setStyle(Paint.Style.FILL);
        iconGlowPaint.setMaskFilter(new android.graphics.BlurMaskFilter(12f, android.graphics.BlurMaskFilter.Blur.NORMAL));
        
        // Paint pour le texte
        textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        textPaint.setColor(Color.WHITE);
        textPaint.setTextSize(28f);
        textPaint.setTextAlign(Paint.Align.CENTER);
        textPaint.setFakeBoldText(true);
        
        // Load drawables
        batteryIcon = ContextCompat.getDrawable(getContext(), R.drawable.ic_battery);
        solarIcon = ContextCompat.getDrawable(getContext(), R.drawable.ic_solar_pannel);
        sector220vIcon = ContextCompat.getDrawable(getContext(), R.drawable.ic_prise_elec_220v);
        chargerExtIcon = ContextCompat.getDrawable(getContext(), R.drawable.ic_ext_charger);
        alternatorIcon = ContextCompat.getDrawable(getContext(), R.drawable.ic_alternator);
        lampIcon = ContextCompat.getDrawable(getContext(), R.drawable.ic_lamp);

        
        // Activer le hardware layer pour les blur
        setLayerType(LAYER_TYPE_SOFTWARE, null);
        
        lastFrameTime = System.currentTimeMillis();
    }
    
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        centerX = w / 2f;
        centerY = h / 2f;
        spacing = Math.min(w, h) / 3.5f;
        iconRadius = Math.min(w, h) / 15f;
    }
    
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        
        // Update animation phase (sine wave for pulsing, 1.8s period)
        long currentTime = System.currentTimeMillis();
        float deltaTime = (currentTime - lastFrameTime) / 1000f; // seconds
        lastFrameTime = currentTime;
        
        if (animationEnabled) {
            pulsePhase += (float) (2 * Math.PI * deltaTime / 1.8f); // 1.8s period
            if (pulsePhase > 2 * Math.PI) {
                pulsePhase -= 2 * Math.PI;
            }
            
            // Flowing energy effect: offset moves from 0 to 1 continuously (2 second cycle)
            flowOffset += deltaTime / 2.0f;
            if (flowOffset > 1.0f) {
                flowOffset -= 1.0f;
            }
        }
        
        // Calculate pulse opacity modifier (+/- 15% using sine wave)
        float pulseOpacity = 1.0f + 0.15f * (float) Math.sin(pulsePhase);
        
        // Positions des sources (gauche)
        float sourceX = centerX - spacing * 1.8f;
        float solar1Y = centerY - spacing * 1.2f;  // Panneau solaire 1 (MPPT 100/50)
        float solar2Y = centerY - spacing * 0.4f;  // Panneau solaire 2 (MPPT 70/15)
        float alternatorY = centerY + spacing * 0.4f;
        float chargerY = centerY + spacing * 1.2f;
        
        // Position de la batterie (centre)
        float batteryX = centerX;
        float batteryY = centerY;
        
        // Positions des destinations (droite)
        float destX = centerX + spacing * 1.8f;
        float system12vY = centerY - spacing * 0.6f;
        float system220vY = centerY + spacing * 0.6f;
        
        // Les puissances des 2 MPPT sont maintenant stockées individuellement
        // (solar1Power et solar2Power sont des variables de classe)
        
        // ═══════════════════════════════════════════════════════════
        // DESSIN DES FLUX D'ÉNERGIE (basé sur les flux explicites calculés)
        // ═══════════════════════════════════════════════════════════
        
        // Sources → Batterie (flux directs)
        if (solar1ToBattery > 0.1f) {
            drawFlow(canvas, sourceX, solar1Y, batteryX, batteryY, solar1ToBattery, MAX_POWER_MPPT_100_50, true, pulseOpacity, iconRadius);
        }
        if (solar2ToBattery > 0.1f) {
            drawFlow(canvas, sourceX, solar2Y, batteryX, batteryY, solar2ToBattery, MAX_POWER_MPPT_70_15, true, pulseOpacity, iconRadius);
        }
        if (alternatorToBattery > 0.1f) {
            drawFlow(canvas, sourceX, alternatorY, batteryX, batteryY, alternatorToBattery, MAX_POWER_ALTERNATOR, true, pulseOpacity, iconRadius);
        }
        if (chargerToBattery > 0.1f) {
            drawFlow(canvas, sourceX, chargerY, batteryX, batteryY, chargerToBattery, MAX_POWER_CHARGER, true, pulseOpacity, iconRadius);
        }
        
        // Sources → 12V (avec waypoint pour contourner batterie)
        if (solar1To12v > 0.1f) {
            drawFlowWithDetour(canvas, sourceX, solar1Y, destX, system12vY, solar1To12v, MAX_POWER_12V, true, pulseOpacity, iconRadius, FlowSource.SOLAR1_TO_220V, batteryX, batteryY);
        }
        if (solar2To12v > 0.1f) {
            drawFlowWithDetour(canvas, sourceX, solar2Y, destX, system12vY, solar2To12v, MAX_POWER_12V, true, pulseOpacity, iconRadius, FlowSource.SOLAR2_TO_220V, batteryX, batteryY);
        }
        if (alternatorTo12v > 0.1f) {
            drawFlowWithDetour(canvas, sourceX, alternatorY, destX, system12vY, alternatorTo12v, MAX_POWER_12V, true, pulseOpacity, iconRadius, FlowSource.ALTERNATOR_TO_12V, batteryX, batteryY);
        }
        if (chargerTo12v > 0.1f) {
            drawFlowWithDetour(canvas, sourceX, chargerY, destX, system12vY, chargerTo12v, MAX_POWER_12V, true, pulseOpacity, iconRadius, FlowSource.SECTEUR_TO_12V, batteryX, batteryY);
        }
        
        // Sources → 220V (avec waypoint pour contourner batterie)
        if (inverterEnabled) {
            if (solar1To220v > 0.1f) {
                drawFlowWithDetour(canvas, sourceX, solar1Y, destX, system220vY, solar1To220v, MAX_POWER_220V, true, pulseOpacity, iconRadius, FlowSource.SOLAR1_TO_220V, batteryX, batteryY);
            }
            if (solar2To220v > 0.1f) {
                drawFlowWithDetour(canvas, sourceX, solar2Y, destX, system220vY, solar2To220v, MAX_POWER_220V, true, pulseOpacity, iconRadius, FlowSource.SOLAR2_TO_220V, batteryX, batteryY);
            }
            if (alternatorTo220v > 0.1f) {
                drawFlowWithDetour(canvas, sourceX, alternatorY, destX, system220vY, alternatorTo220v, MAX_POWER_220V, true, pulseOpacity, iconRadius, FlowSource.ALTERNATOR_TO_12V, batteryX, batteryY);
            }
            if (chargerTo220v > 0.1f) {
                drawFlowWithDetour(canvas, sourceX, chargerY, destX, system220vY, chargerTo220v, MAX_POWER_220V, true, pulseOpacity, iconRadius, FlowSource.SECTEUR_TO_12V, batteryX, batteryY);
            }
        }
        
        // Batterie → Charges (flux sortants depuis batterie)
        if (batteryTo12v > 0.1f) {
            drawFlowWithDetour(canvas, batteryX, batteryY, destX, system12vY, batteryTo12v, MAX_POWER_12V, false, pulseOpacity, iconRadius, FlowSource.ALTERNATOR_TO_12V, batteryX, batteryY);
        }
        if (batteryTo220v > 0.1f && inverterEnabled) {
            drawFlowWithDetour(canvas, batteryX, batteryY, destX, system220vY, batteryTo220v, MAX_POWER_220V, false, pulseOpacity, iconRadius, FlowSource.SOLAR1_TO_220V, batteryX, batteryY);
        }
        
        // ═══════════════════════════════════════════════════════════
        // DESSIN DES IC\u00d4NES ET LABELS
        // ═══════════════════════════════════════════════════════════
        float correctedBatteryPower = batteryPower;
        
        // Dessiner les icônes et labels
        // Sources: halo animé de l'extérieur vers l'intérieur (captent l'énergie)
        drawIconWithDrawable(canvas, sourceX, solar1Y, colorSolar, solarIcon, solar1Power, "MPPT 100/50", true, DeviceType.SOURCE);
        drawIconWithDrawable(canvas, sourceX, solar2Y, colorSolar, solarIcon, solar2Power, "MPPT 70/15", true, DeviceType.SOURCE);
        drawIconWithDrawable(canvas, sourceX, alternatorY, colorAlternator, alternatorIcon, alternatorPower, "Alternator", true, DeviceType.SOURCE);
        drawIconWithDrawable(canvas, sourceX, chargerY, colorCharger, chargerExtIcon, chargerPower, "Sector AC", true, DeviceType.SOURCE);
        
        // Batterie: pas d'animation de halo (neutre)
        // Afficher la puissance avec le signe (+ charge, - décharge)
        drawIconWithDrawable(canvas, batteryX, batteryY, colorBattery, batteryIcon, batteryPower, "", false, DeviceType.NEUTRAL);
        
        // Charges: halo animé de l'intérieur vers l'extérieur (consomment l'énergie)
        drawIconWithDrawable(canvas, destX, system12vY, color12V, lampIcon, system12vPower, "DC Devices", false, DeviceType.LOAD);
        drawIconWithDrawable(canvas, destX, system220vY, color220V, sector220vIcon, system220vPower, "AC Devices" + (inverterEnabled ? "" : " (OFF)"), false, DeviceType.LOAD);
        
        // Draw state text on battery
        String stateText;
        int stateColor;
        if (correctedBatteryPower > 0.1f) {
            stateText = "CHARGING";
            stateColor = colorIncomingStart;
        } else if (correctedBatteryPower < -0.1f) {
            stateText = "DISCHARGING";
            stateColor = colorOutgoingStart;
        } else {
            stateText = "IDLE";
            stateColor = Color.WHITE;
        }
        textPaint.setTextSize(iconRadius * 0.35f);
        textPaint.setColor(stateColor);
        canvas.drawText(stateText, batteryX, batteryY + iconRadius * 2.2f, textPaint);
        
        // Animation continue
        if (animationEnabled) {
            invalidate();
        }
    }
    
    /**
     * Dessine un flux d'énergie avec contournement de la batterie
     * @param source Identifie le type de flux pour choisir le bon template de waypoints
     */
    private void drawFlowWithDetour(Canvas canvas, float x1, float y1, float x2, float y2, float power, float maxPowerForSource, boolean isIncoming, float pulseOpacity, float clipRadius, FlowSource source, float batteryX, float batteryY) {
        if (power < 0.1f) return;
        
        // Zone d'exclusion autour de la batterie
        float batteryZoneRadius = iconRadius * 2.0f;
        
        // Vérifier si le segment traverse la zone de la batterie
        // Calculer la distance minimale entre le segment et le centre de la batterie
        boolean crossesBattery = false;
        
        // Si le flux vient de la batterie ou va vers la batterie, pas de détour
        float distToBatteryStart = (float) Math.sqrt(Math.pow(x1 - batteryX, 2) + Math.pow(y1 - batteryY, 2));
        float distToBatteryEnd = (float) Math.sqrt(Math.pow(x2 - batteryX, 2) + Math.pow(y2 - batteryY, 2));
        
        if (distToBatteryStart < iconRadius * 1.5f || distToBatteryEnd < iconRadius * 1.5f) {
            // Le flux part de ou arrive à la batterie, pas de détour
            drawFlow(canvas, x1, y1, x2, y2, power, maxPowerForSource, isIncoming, pulseOpacity, clipRadius);
            return;
        }
        
        // Calculer la distance perpendiculaire du segment au centre de la batterie
        float dx = x2 - x1;
        float dy = y2 - y1;
        float length = (float) Math.sqrt(dx * dx + dy * dy);
        
        if (length > 0) {
            // Vecteur normalisé du segment
            float nx = dx / length;
            float ny = dy / length;
            
            // Vecteur de x1 au centre de la batterie
            float toBatteryX = batteryX - x1;
            float toBatteryY = batteryY - y1;
            
            // Projection sur le segment
            float projection = toBatteryX * nx + toBatteryY * ny;
            
            // Point le plus proche sur le segment
            if (projection > 0 && projection < length) {
                float closestX = x1 + nx * projection;
                float closestY = y1 + ny * projection;
                
                float distToBattery = (float) Math.sqrt(
                    Math.pow(closestX - batteryX, 2) + Math.pow(closestY - batteryY, 2)
                );
                
                if (distToBattery < batteryZoneRadius) {
                    crossesBattery = true;
                }
            }
        }
        
        if (crossesBattery) {
            // Contournement avec waypoints - créer une liste de points de passage
            float clearance = iconRadius * 3.0f; // Distance de dégagement
            float[] waypoints = createDetourWaypoints(x1, y1, x2, y2, source, batteryX, batteryY, clearance);
            
            // Dessiner le flux le long des waypoints avec spline
            drawFlowAlongWaypoints(canvas, waypoints, power, maxPowerForSource, isIncoming, pulseOpacity, clipRadius);
        } else {
            // Dessin direct (pas de contournement)
            drawFlow(canvas, x1, y1, x2, y2, power, maxPowerForSource, isIncoming, pulseOpacity, clipRadius);
        }
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // CONFIGURATION DES WAYPOINTS POUR CONTOURNEMENT DE LA BATTERIE
    // Pour éditer les trajectoires, modifier les valeurs ci-dessous
    // Format: ratios X (0.0 = source, 1.0 = destination) et offsets Y
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Énumération des sources de flux pour identifier le waypoint template à utiliser
    private enum FlowSource {
        SOLAR1_TO_220V,      // MPPT 100/50 → 220V
        SOLAR2_TO_220V,      // MPPT 70/15 → 220V
        ALTERNATOR_TO_12V,   // Alternateur → 12V
        SECTEUR_TO_12V       // Secteur 220V → 12V
    }
    
    // Device type for halo animation
    private enum DeviceType {
        SOURCE,   // Energy comes from outside (halo: outside→inside)
        LOAD,     // Energy goes outside (halo: inside→outside)
        NEUTRAL   // No animated halo (battery)
    }
    
    // CAS 1: Solar MPPT 100/50 → 220V (passe AU-DESSUS, descend vers 220V)
    private static final float[] WAYPOINT_SOLAR1_TO_220V = {
        // Point de départ ajouté automatiquement
        0.25f,  // X: 25% du chemin horizontal
        -1.3f,  // Y: au-dessus de la batterie (clearance)
        0.75f,  // X: 75% du chemin
        -0.7f,  // Y: reste au-dessus
        // Point d'arrivée ajouté automatiquement
    };
    
    // CAS 2: Solar MPPT 70/15 → 220V (passe AU-DESSUS, descend vers 220V)
    private static final float[] WAYPOINT_SOLAR2_TO_220V = {
        0.2f,  // X: 25% du chemin
        -0.8f,  // Y: au-dessus de la batterie
        0.75f,  // X: 75% du chemin
        -0.3f,  // Y: reste au-dessus
    };
    
    // CAS 3: Alternator → 12V devices (passe EN-DESSOUS, remonte vers 12V)
    private static final float[] WAYPOINT_ALTERNATOR_TO_12V = {
        0.25f,  // X: 25% du chemin
        1.0f,   // Y: sous la batterie (clearance)
        0.75f,  // X: 75% du chemin
        1.5f,   // Y: reste en-dessous
        0.95f,  // X: 75% du chemin
        -0.3f,   // Y: reste en-dessous
    };
    
    // CAS 4: Secteur 220V → 12V devices (passe EN-DESSOUS, remonte vers 12V)
    private static final float[] WAYPOINT_SECTEUR_TO_12V = {
        0.25f,  // X: 25% du chemin
        2.0f,   // Y: sous la batterie (clearance)
        0.6f,   // X: 60% du chemin
        1.7f,   // Y: reste en-dessous
        0.85f,  // X: 85% du chemin
        -0.4f,   // Y: reste en-dessous

    };
    
    /**
     * Crée une liste de waypoints pour contourner la batterie
     * @return tableau [x1, y1, x2, y2, x3, y3, ...] des points de passage
     */
    private float[] createDetourWaypoints(float x1, float y1, float x2, float y2, FlowSource source, float batteryX, float batteryY, float clearance) {
        // Choisir le bon template de waypoints selon la source
        float[] template;
        switch (source) {
            case SOLAR1_TO_220V:
                template = WAYPOINT_SOLAR1_TO_220V;
                break;
            case SOLAR2_TO_220V:
                template = WAYPOINT_SOLAR2_TO_220V;
                break;
            case ALTERNATOR_TO_12V:
                template = WAYPOINT_ALTERNATOR_TO_12V;
                break;
            case SECTEUR_TO_12V:
                template = WAYPOINT_SECTEUR_TO_12V;
                break;
            default:
                // Fallback sur Solar1 si source inconnue
                template = WAYPOINT_SOLAR1_TO_220V;
                break;
        }
        
        // Convertir le template en coordonnées absolues
        int numWaypoints = template.length / 2;
        float[] waypoints = new float[(numWaypoints + 2) * 2]; // +2 pour départ et arrivée
        
        // Point de départ
        waypoints[0] = x1;
        waypoints[1] = y1;
        
        // Points intermédiaires
        float dx = x2 - x1;
        for (int i = 0; i < numWaypoints; i++) {
            float ratioX = template[i * 2];
            float ratioY = template[i * 2 + 1];
            
            waypoints[(i + 1) * 2] = x1 + dx * ratioX;
            waypoints[(i + 1) * 2 + 1] = batteryY + clearance * ratioY;
        }
        
        // Point d'arrivée
        waypoints[(numWaypoints + 1) * 2] = x2;
        waypoints[(numWaypoints + 1) * 2 + 1] = y2;
        
        return waypoints;
    }
    
    /**
     * Dessine un flux le long d'une série de waypoints avec interpolation spline Catmull-Rom
     */
    private void drawFlowAlongWaypoints(Canvas canvas, float[] waypoints, float power, float maxPowerForSource, boolean isIncoming, float pulseOpacity, float clipRadius) {
        if (power < 0.1f || waypoints == null || waypoints.length < 4) return;
        
        // Si seulement 2 points, utiliser le dessin direct
        int numPoints = waypoints.length / 2;
        if (numPoints == 2) {
            drawFlow(canvas, waypoints[0], waypoints[1], waypoints[2], waypoints[3], power, maxPowerForSource, isIncoming, pulseOpacity, clipRadius);
            return;
        }
        
        // Convertir en listes de coordonnées
        float[] xPoints = new float[numPoints];
        float[] yPoints = new float[numPoints];
        for (int i = 0; i < numPoints; i++) {
            xPoints[i] = waypoints[i * 2];
            yPoints[i] = waypoints[i * 2 + 1];
        }
        
        // Générer la spline Catmull-Rom avec interpolation
        int segmentsPerWaypoint = 20;
        float[] splineX = new float[segmentsPerWaypoint * (numPoints - 1) + 1];
        float[] splineY = new float[segmentsPerWaypoint * (numPoints - 1) + 1];
        
        int splineIndex = 0;
        for (int i = 0; i < numPoints - 1; i++) {
            // Points de contrôle Catmull-Rom (4 points pour interpoler entre p1 et p2)
            float p0x = (i > 0) ? xPoints[i - 1] : xPoints[i];
            float p0y = (i > 0) ? yPoints[i - 1] : yPoints[i];
            float p1x = xPoints[i];
            float p1y = yPoints[i];
            float p2x = xPoints[i + 1];
            float p2y = yPoints[i + 1];
            float p3x = (i < numPoints - 2) ? xPoints[i + 2] : xPoints[i + 1];
            float p3y = (i < numPoints - 2) ? yPoints[i + 2] : yPoints[i + 1];
            
            // Interpolation Catmull-Rom pour ce segment
            for (int j = 0; j < segmentsPerWaypoint; j++) {
                float t = (float) j / segmentsPerWaypoint;
                float t2 = t * t;
                float t3 = t2 * t;
                
                splineX[splineIndex] = 0.5f * (
                    (2f * p1x) +
                    (-p0x + p2x) * t +
                    (2f * p0x - 5f * p1x + 4f * p2x - p3x) * t2 +
                    (-p0x + 3f * p1x - 3f * p2x + p3x) * t3
                );
                splineY[splineIndex] = 0.5f * (
                    (2f * p1y) +
                    (-p0y + p2y) * t +
                    (2f * p0y - 5f * p1y + 4f * p2y - p3y) * t2 +
                    (-p0y + 3f * p1y - 3f * p2y + p3y) * t3
                );
                splineIndex++;
            }
        }
        // Dernier point
        splineX[splineIndex] = xPoints[numPoints - 1];
        splineY[splineIndex] = yPoints[numPoints - 1];
        splineIndex++;
        
        // Dessiner le ruban le long de la spline
        drawSplineRibbon(canvas, splineX, splineY, splineIndex, power, maxPowerForSource, isIncoming, pulseOpacity, clipRadius);
    }
    
    /**
     * Dessine un ruban de largeur constante le long d'une courbe spline
     */
    private void drawSplineRibbon(Canvas canvas, float[] splineX, float[] splineY, int numPoints, float power, float maxPowerForSource, boolean isIncoming, float pulseOpacity, float clipRadius) {
        if (numPoints < 2) return;
        
        // Calculer largeur et couleurs
        float maxWidth = iconRadius * 1.5f;
        float minWidth = 8f;
        float flowWidth = Math.min(Math.max(power / maxPowerForSource * maxWidth, minWidth), maxWidth);
        
        int colorStart, colorEnd, colorGlow;
        if (isIncoming) {
            colorStart = colorIncomingStart;
            colorEnd = colorIncomingEnd;
            colorGlow = colorIncomingGlow;
        } else {
            colorStart = colorOutgoingStart;
            colorEnd = colorOutgoingEnd;
            colorGlow = colorOutgoingGlow;
        }
        
        if (power > 100f) {
            float[] hsvStart = new float[3];
            float[] hsvEnd = new float[3];
            Color.colorToHSV(colorStart, hsvStart);
            Color.colorToHSV(colorEnd, hsvEnd);
            hsvStart[1] = Math.min(hsvStart[1] * 1.2f, 1.0f);
            hsvEnd[1] = Math.min(hsvEnd[1] * 1.2f, 1.0f);
            colorStart = Color.HSVToColor(hsvStart);
            colorEnd = Color.HSVToColor(hsvEnd);
        }
        
        // Calculer normales perpendiculaires à chaque point
        float[] normalsX = new float[numPoints];
        float[] normalsY = new float[numPoints];
        
        for (int i = 0; i < numPoints; i++) {
            float dx, dy;
            if (i == 0) {
                dx = splineX[1] - splineX[0];
                dy = splineY[1] - splineY[0];
            } else if (i == numPoints - 1) {
                dx = splineX[i] - splineX[i - 1];
                dy = splineY[i] - splineY[i - 1];
            } else {
                dx = splineX[i + 1] - splineX[i - 1];
                dy = splineY[i + 1] - splineY[i - 1];
            }
            
            float length = (float) Math.sqrt(dx * dx + dy * dy);
            if (length > 0) {
                normalsX[i] = -dy / length;
                normalsY[i] = dx / length;
            } else {
                normalsX[i] = 0;
                normalsY[i] = 0;
            }
        }
        
        // Construire le path du ruban
        Path flowPath = new Path();
        
        // Contour supérieur
        flowPath.moveTo(splineX[0] + normalsX[0] * flowWidth / 2, splineY[0] + normalsY[0] * flowWidth / 2);
        for (int i = 1; i < numPoints; i++) {
            flowPath.lineTo(splineX[i] + normalsX[i] * flowWidth / 2, splineY[i] + normalsY[i] * flowWidth / 2);
        }
        
        // Contour inférieur (sens inverse)
        for (int i = numPoints - 1; i >= 0; i--) {
            flowPath.lineTo(splineX[i] - normalsX[i] * flowWidth / 2, splineY[i] - normalsY[i] * flowWidth / 2);
        }
        flowPath.close();
        
        // Clipping pour les cercles de départ/arrivée
        canvas.save();
        if (clipRadius > 0) {
            Path clipPath = new Path();
            clipPath.addCircle(splineX[0], splineY[0], clipRadius, Path.Direction.CW);
            clipPath.addCircle(splineX[numPoints - 1], splineY[numPoints - 1], clipRadius, Path.Direction.CW);
            canvas.clipPath(clipPath, android.graphics.Region.Op.DIFFERENCE);
        }
        
        // Apply pulse animation to opacity
        float baseOpacity = 0.5f; // 50% transparency on ribbon (match drawFlowSegment)
        float glowOpacity = 0.3f; // 30% for inner glow
        
        float finalOpacity = baseOpacity * pulseOpacity;
        float finalGlowOpacity = glowOpacity * pulseOpacity;
        
        // 1. Draw inner glow (30% opacity, blur 12dp)
        int glowAlpha = (int) (255 * finalGlowOpacity);
        flowInnerGlowPaint.setColor((colorGlow & 0x00FFFFFF) | (glowAlpha << 24));
        canvas.drawPath(flowPath, flowInnerGlowPaint);
        
        // 2. Draw main ribbon with animated flowing gradient (50% transparency)
        // Create flowing energy effect with 3 bright "packets" traveling along the path
        // Each packet travels from 0 to 1, then wraps back to 0
        float pos1 = flowOffset;
        float pos2 = (flowOffset + 0.25f) % 1.0f;
        float pos3 = (flowOffset + 0.5f) % 1.0f;
        
        float packetWidth = 0.08f; // Width of each bright packet
        float dimOpacity = finalOpacity * 0.5f;  // Dim base
        float brightOpacity = finalOpacity * 1.5f; // Bright packets
        
        // Build gradient with packets - sort positions to handle wrap-around properly
        float[] packets = {pos1, pos2, pos3};
        java.util.Arrays.sort(packets);
        
        java.util.ArrayList<Integer> colors = new java.util.ArrayList<>();
        java.util.ArrayList<Float> stops = new java.util.ArrayList<>();
        
        colors.add((colorStart & 0x00FFFFFF) | ((int)(255 * dimOpacity) << 24));
        stops.add(0.0f);
        
        // Add each packet (with fade in/out)
        for (float pos : packets) {
            // Fade in before packet
            if (pos > packetWidth) {
                colors.add((colorStart & 0x00FFFFFF) | ((int)(255 * dimOpacity) << 24));
                stops.add(Math.max(0.0f, pos - packetWidth));
            }
            // Bright center
            colors.add((colorStart & 0x00FFFFFF) | ((int)(255 * brightOpacity) << 24));
            stops.add(pos);
            // Fade out after packet
            if (pos < 1.0f - packetWidth) {
                colors.add((colorStart & 0x00FFFFFF) | ((int)(255 * dimOpacity) << 24));
                stops.add(Math.min(1.0f, pos + packetWidth));
            }
        }
        
        colors.add((colorEnd & 0x00FFFFFF) | ((int)(255 * dimOpacity) << 24));
        stops.add(1.0f);
        
        int[] colorArray = new int[colors.size()];
        float[] stopArray = new float[stops.size()];
        for (int i = 0; i < colors.size(); i++) {
            colorArray[i] = colors.get(i);
            stopArray[i] = stops.get(i);
        }
        
        LinearGradient gradient = new LinearGradient(
            splineX[0], splineY[0], splineX[numPoints - 1], splineY[numPoints - 1],
            colorArray,
            stopArray,
            Shader.TileMode.CLAMP
        );
        flowPaint.setShader(gradient);
        canvas.drawPath(flowPath, flowPaint);
        flowPaint.setShader(null);
        
        // 3. Draw outer stroke (100% solid)
        flowOuterStrokePaint.setColor(colorGlow);
        canvas.drawPath(flowPath, flowOuterStrokePaint);
        
        canvas.restore();
    }
    
    private void drawFlow(Canvas canvas, float x1, float y1, float x2, float y2, float power, float maxPowerForSource, boolean isIncoming, float pulseOpacity, float clipRadius) {
        drawFlowSegment(canvas, x1, y1, x2, y2, power, maxPowerForSource, isIncoming, pulseOpacity, clipRadius, clipRadius);
    }
    
    private void drawFlowSegment(Canvas canvas, float x1, float y1, float x2, float y2, float power, float maxPowerForSource, boolean isIncoming, float pulseOpacity, float clipRadiusStart, float clipRadiusEnd) {
        if (power < 0.1f) return;
        
        // Determine if flow is active (> 10W for active styling)
        boolean isActive = power > 10f;
        
        // Calculate flow width proportional to power
        // Max width = 3/4 of circle diameter at maxPowerForSource
        // Max width = (iconRadius * 2) * 0.75 = iconRadius * 1.5
        float maxWidth = iconRadius * 1.5f;
        float minWidth = 8f; // Largeur minimale pour permettre aux petits flux d'être visibles mais fins
        float flowWidth = Math.min(Math.max(power / maxPowerForSource * maxWidth, minWidth), maxWidth);
        
        // Select colors based on flow direction
        int colorStart, colorEnd, colorGlow;
        if (isIncoming) {
            // Incoming energy (to battery): Cyan gradient
            colorStart = colorIncomingStart;
            colorEnd = colorIncomingEnd;
            colorGlow = colorIncomingGlow;
        } else {
            // Outgoing energy (from battery): Orange gradient
            colorStart = colorOutgoingStart;
            colorEnd = colorOutgoingEnd;
            colorGlow = colorOutgoingGlow;
        }
        
        // Apply saturation boost for high power flows (> 100W)
        if (power > 100f) {
            float[] hsvStart = new float[3];
            float[] hsvEnd = new float[3];
            Color.colorToHSV(colorStart, hsvStart);
            Color.colorToHSV(colorEnd, hsvEnd);
            hsvStart[1] = Math.min(hsvStart[1] * 1.2f, 1.0f); // +20% saturation
            hsvEnd[1] = Math.min(hsvEnd[1] * 1.2f, 1.0f);
            colorStart = Color.HSVToColor(hsvStart);
            colorEnd = Color.HSVToColor(hsvEnd);
        }
        
        // Calculate flow direction
        float dx = x2 - x1;
        float dy = y2 - y1;
        float distance = (float) Math.sqrt(dx * dx + dy * dy);
        
        // Create smooth Bezier curve path from center to center
        Path flowPath = new Path();
        float controlX = (x1 + x2) / 2f;
        float controlY = (y1 + y2) / 2f;
        
        // Calculate perpendicular offset for ribbon width
        float angle = (float) Math.atan2(dy, dx);
        float offsetX = (float) (Math.sin(angle) * flowWidth / 2);
        float offsetY = (float) (-Math.cos(angle) * flowWidth / 2);
        
        // Top edge of ribbon
        flowPath.moveTo(x1 + offsetX, y1 + offsetY);
        flowPath.cubicTo(
            controlX + offsetX, y1 + offsetY,
            controlX + offsetX, y2 + offsetY,
            x2 + offsetX, y2 + offsetY
        );
        
        // Bottom edge of ribbon (return path)
        flowPath.lineTo(x2 - offsetX, y2 - offsetY);
        flowPath.cubicTo(
            controlX - offsetX, y2 - offsetY,
            controlX - offsetX, y1 - offsetY,
            x1 - offsetX, y1 - offsetY
        );
        flowPath.close();
        
        // Save canvas state for clipping
        canvas.save();
        
        // Create clip paths for source and destination circles
        Path clipPath = new Path();
        boolean hasClipping = false;
        
        if (clipRadiusStart > 0) {
            clipPath.addCircle(x1, y1, clipRadiusStart, Path.Direction.CW);
            hasClipping = true;
        }
        if (clipRadiusEnd > 0) {
            clipPath.addCircle(x2, y2, clipRadiusEnd, Path.Direction.CW);
            hasClipping = true;
        }
        
        if (hasClipping) {
            // Clip out the circles (inverse clip)
            canvas.clipPath(clipPath, android.graphics.Region.Op.DIFFERENCE);
        }
        
        if (isActive) {
            // ACTIVE FLOW: Full rendering with glow and pulse
            
            // Apply pulse animation to opacity
            float baseOpacity = 0.5f; // 50% transparency on ribbon
            float glowOpacity = 0.3f; // 30% for inner glow
            
            float finalOpacity = baseOpacity * pulseOpacity;
            float finalGlowOpacity = glowOpacity * pulseOpacity;
            
            // 1. Draw inner glow (30% opacity, blur 12dp)
            int glowAlpha = (int) (255 * finalGlowOpacity);
            flowInnerGlowPaint.setColor((colorGlow & 0x00FFFFFF) | (glowAlpha << 24));
            canvas.drawPath(flowPath, flowInnerGlowPaint);
            
            // 2. Draw main ribbon with animated flowing gradient (50% transparency)
            // Create flowing energy effect with 3 bright "packets" traveling along the path
            // Each packet travels from 0 to 1, then wraps back to 0
            float pos1 = flowOffset;
            float pos2 = (flowOffset + 0.25f) % 1.0f;
            float pos3 = (flowOffset + 0.5f) % 1.0f;
            
            float packetWidth = 0.08f; // Width of each bright packet
            float dimOpacity = finalOpacity * 0.5f;  // Dim base
            float brightOpacity = finalOpacity * 1.5f; // Bright packets
            
            // Build gradient with packets - sort positions to handle wrap-around
            float[] packets = {pos1, pos2, pos3};
            java.util.Arrays.sort(packets);
            
            java.util.ArrayList<Integer> colors = new java.util.ArrayList<>();
            java.util.ArrayList<Float> stops = new java.util.ArrayList<>();
            
            colors.add((colorStart & 0x00FFFFFF) | ((int)(255 * dimOpacity) << 24));
            stops.add(0.0f);
            
            // Add each packet (with fade in/out)
            for (float pos : packets) {
                // Fade in before packet
                if (pos > packetWidth) {
                    colors.add((colorStart & 0x00FFFFFF) | ((int)(255 * dimOpacity) << 24));
                    stops.add(Math.max(0.0f, pos - packetWidth));
                }
                // Bright center
                colors.add((colorStart & 0x00FFFFFF) | ((int)(255 * brightOpacity) << 24));
                stops.add(pos);
                // Fade out after packet
                if (pos < 1.0f - packetWidth) {
                    colors.add((colorStart & 0x00FFFFFF) | ((int)(255 * dimOpacity) << 24));
                    stops.add(Math.min(1.0f, pos + packetWidth));
                }
            }
            
            colors.add((colorEnd & 0x00FFFFFF) | ((int)(255 * dimOpacity) << 24));
            stops.add(1.0f);
            
            int[] colorArray = new int[colors.size()];
            float[] stopArray = new float[stops.size()];
            for (int i = 0; i < colors.size(); i++) {
                colorArray[i] = colors.get(i);
                stopArray[i] = stops.get(i);
            }
            
            LinearGradient gradient = new LinearGradient(
                x1, y1, x2, y2,
                colorArray,
                stopArray,
                Shader.TileMode.CLAMP
            );
            flowPaint.setShader(gradient);
            canvas.drawPath(flowPath, flowPaint);
            flowPaint.setShader(null);
            
            // 3. Draw outer stroke (100% solid) - only on the path outline
            flowOuterStrokePaint.setColor(colorGlow);
            canvas.drawPath(flowPath, flowOuterStrokePaint);
            
        } else {
            // INACTIVE FLOW: 40% opacity, no glow, no pulse
            int inactiveAlpha = (int) (255 * 0.4f);
            
            // Draw main ribbon
            LinearGradient gradient = new LinearGradient(
                x1, y1, x2, y2,
                new int[]{
                    (colorStart & 0x00FFFFFF) | (inactiveAlpha << 24),
                    (colorEnd & 0x00FFFFFF) | (inactiveAlpha << 24)
                },
                null,
                Shader.TileMode.CLAMP
            );
            flowPaint.setShader(gradient);
            canvas.drawPath(flowPath, flowPaint);
            flowPaint.setShader(null);
            
            // Draw stroke (solid with same alpha as fill)
            flowOuterStrokePaint.setColor((colorGlow & 0x00FFFFFF) | (inactiveAlpha << 24));
            canvas.drawPath(flowPath, flowOuterStrokePaint);
        }
        
        // Restore canvas state (remove clip)
        canvas.restore();
    }
    
    private void drawIconWithDrawable(Canvas canvas, float x, float y, int color, Drawable icon, float power, String label, boolean isLeftSide, DeviceType deviceType) {
        // Draw animated glow circle (with blur)
        // Use sine wave for smooth looping animation (no jumps)
        float haloRadius;
        float haloOpacity;
        
        // Convert flowOffset (0→1) to sine wave (-1→1→-1) for smooth looping
        float sineWave = (float) Math.sin(flowOffset * 2 * Math.PI);
        
        if (deviceType == DeviceType.SOURCE && power > 0.1f) {
            // Source: halo pulse from outside to inside (capturing energy)
            // Sine oscillates: large & dim ↔ small & bright
            float radiusVariation = sineWave * 0.15f; // ±0.15
            haloRadius = iconRadius * (1.65f - radiusVariation); // 1.5 ↔ 1.8
            haloOpacity = 0.22f + sineWave * 0.08f; // 14% ↔ 30%
        } else if (deviceType == DeviceType.LOAD && power > 0.1f) {
            // Load: halo pulse from inside to outside (emitting energy)
            // Inverse sine: small & bright ↔ large & dim
            float radiusVariation = sineWave * 0.15f; // ±0.15
            haloRadius = iconRadius * (1.65f + radiusVariation); // 1.5 ↔ 1.8
            haloOpacity = 0.22f - sineWave * 0.08f; // 14% ↔ 30%
        } else {
            // Neutral or inactive: static halo
            haloRadius = iconRadius * 1.5f;
            haloOpacity = 0.25f;
        }
        
        int haloAlpha = (int) (255 * haloOpacity);
        iconGlowPaint.setColor((color & 0x00FFFFFF) | (haloAlpha << 24));
        canvas.drawCircle(x, y, haloRadius, iconGlowPaint);
        
        // Draw transparent circle background
        canvas.drawCircle(x, y, iconRadius, iconCirclePaint);
        
        // Draw outline circle
        iconCircleStrokePaint.setColor(color);
        canvas.drawCircle(x, y, iconRadius, iconCircleStrokePaint);
        
        // Draw icon (drawable)
        if (icon != null) {
            int iconSize = (int) (iconRadius * 1.2f);
            int left = (int) (x - iconSize / 2);
            int top = (int) (y - iconSize / 2);
            int right = left + iconSize;
            int bottom = top + iconSize;
            
            icon.setBounds(left, top, right, bottom);
            int white = Color.parseColor("#FFFFFF");
            icon.setColorFilter(new PorterDuffColorFilter(white, PorterDuff.Mode.SRC_IN));
            icon.draw(canvas);
        }
        
        // Draw power value below icon
        if (Math.abs(power) > 0.1f) {
            textPaint.setTextSize(iconRadius * 0.45f);
            textPaint.setColor(Color.WHITE);
            // Afficher le signe pour les valeurs positives et négatives
            String powerText = String.format("%+.0fW", power);
            canvas.drawText(powerText, x, y + iconRadius * 1.6f, textPaint);
        }
        
        // Draw label next to icon
        if (label != null && !label.isEmpty()) {
            textPaint.setTextSize(iconRadius * 0.4f);
            textPaint.setColor(0xAAFFFFFF); // Semi-transparent white
            if (isLeftSide) {
                // Label on the left of the circle
                textPaint.setTextAlign(Paint.Align.RIGHT);
                canvas.drawText(label, x - iconRadius * 1.4f, y + iconRadius * 0.15f, textPaint);
            } else {
                // Label on the right of the circle
                textPaint.setTextAlign(Paint.Align.LEFT);
                canvas.drawText(label, x + iconRadius * 1.4f, y + iconRadius * 0.15f, textPaint);
            }
            // Reset text align to center for other texts
            textPaint.setTextAlign(Paint.Align.CENTER);
        }
    }
    
    // Méthodes pour mettre à jour les données
    
    public void setSolarPower(float mppt10050Power, float mppt7015Power) {
        this.solar1Power = Math.max(0, mppt10050Power);
        this.solar2Power = Math.max(0, mppt7015Power);
        this.solarPower = this.solar1Power + this.solar2Power;
        invalidate();
    }
    
    public void setAlternatorPower(float power) {
        this.alternatorPower = Math.max(0, power);
        invalidate();
    }
    
    public void setChargerPower(float power) {
        this.chargerPower = Math.max(0, power);
        invalidate();
    }
    
    public void setSystem12vPower(float power) {
        this.system12vPower = Math.max(0, power);
        invalidate();
    }
    
    public void setSystem220vPower(float power) {
        this.system220vPower = Math.max(0, power);
        invalidate();
    }
    
    public void setBatteryPower(float power) {
        this.batteryPower = power;
        invalidate();
    }
    
    public void setAnimationEnabled(boolean enabled) {
        this.animationEnabled = enabled;
        if (enabled) {
            invalidate();
        }
    }
    
    public void setInverterEnabled(boolean enabled) {
        this.inverterEnabled = enabled;
        invalidate();
    }
    
    public void set220vPassthrough(boolean passthrough) {
        this.is220vPassthrough = passthrough;
        invalidate();
    }
    
    /**
     * Set explicit energy flows based on real measurements from sensors.
     * All flows in Watts. This replaces the ratio-based guessing logic.
     */
    public void setEnergyFlows(
        float solar1ToBattery, float solar2ToBattery,
        float solar1To12v, float solar2To12v,
        float solar1To220v, float solar2To220v,
        float alternatorToBattery, float alternatorTo12v, float alternatorTo220v,
        float chargerToBattery, float chargerTo12v, float chargerTo220v,
        float batteryTo12v, float batteryTo220v
    ) {
        this.solar1ToBattery = Math.max(0, solar1ToBattery);
        this.solar2ToBattery = Math.max(0, solar2ToBattery);
        this.solar1To12v = Math.max(0, solar1To12v);
        this.solar2To12v = Math.max(0, solar2To12v);
        this.solar1To220v = Math.max(0, solar1To220v);
        this.solar2To220v = Math.max(0, solar2To220v);
        this.alternatorToBattery = Math.max(0, alternatorToBattery);
        this.alternatorTo12v = Math.max(0, alternatorTo12v);
        this.alternatorTo220v = Math.max(0, alternatorTo220v);
        this.chargerToBattery = Math.max(0, chargerToBattery);
        this.chargerTo12v = Math.max(0, chargerTo12v);
        this.chargerTo220v = Math.max(0, chargerTo220v);
        this.batteryTo12v = Math.max(0, batteryTo12v);
        this.batteryTo220v = Math.max(0, batteryTo220v);
        invalidate();
    }
}
