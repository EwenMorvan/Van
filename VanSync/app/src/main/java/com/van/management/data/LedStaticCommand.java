package com.van.management.data;

import java.nio.ByteBuffer;

/**
 * Représentation Java de la structure led_static_command_t
 * Commande statique pour les LEDs (Roof ou Ext)
 */
public class LedStaticCommand {
    
    // Cibles pour STATIC (Roof + Ext)
    public enum StaticTarget {
        ROOF_LED1(0),
        ROOF_LED2(1),
        ROOF_LED_ALL(2),
        EXT_AV_LED(3),
        EXT_AR_LED(4),
        EXT_LED_ALL(5);
        
        private final int value;
        StaticTarget(int value) { this.value = value; }
        public int getValue() { return value; }
        
        public boolean isRoof() {
            return this == ROOF_LED1 || this == ROOF_LED2 || this == ROOF_LED_ALL;
        }
        
        public boolean isExt() {
            return this == EXT_AV_LED || this == EXT_AR_LED || this == EXT_LED_ALL;
        }
    }
    
    private StaticTarget target;
    private LedData[] roof1Colors;  // Pour ROOF targets
    private LedData[] roof2Colors;
    private LedData[] extAvColors;  // Pour EXT targets
    private LedData[] extArColors;
    
    // Tailles des strips (configuration réelle)
    public static final int LED_STRIP_1_COUNT = 120;
    public static final int LED_STRIP_2_COUNT = 120;
    public static final int LED_STRIP_EXT_FRONT_COUNT = 60;
    public static final int LED_STRIP_EXT_BACK_COUNT = 10;
    
    private LedStaticCommand(StaticTarget target) {
        this.target = target;
    }
    
    // Factory pour commandes Roof
    public static LedStaticCommand createRoof(StaticTarget target, LedData[] roof1Colors, LedData[] roof2Colors) {
        if (!target.isRoof()) {
            throw new IllegalArgumentException("Target must be a ROOF target");
        }
        if (roof1Colors.length != LED_STRIP_1_COUNT || roof2Colors.length != LED_STRIP_2_COUNT) {
            throw new IllegalArgumentException("Invalid array sizes for roof LEDs");
        }
        
        LedStaticCommand cmd = new LedStaticCommand(target);
        cmd.roof1Colors = roof1Colors;
        cmd.roof2Colors = roof2Colors;
        return cmd;
    }
    
    // Factory pour commandes Ext
    public static LedStaticCommand createExt(StaticTarget target, LedData[] extAvColors, LedData[] extArColors) {
        if (!target.isExt()) {
            throw new IllegalArgumentException("Target must be an EXT target");
        }
        if (extAvColors.length != LED_STRIP_EXT_FRONT_COUNT || extArColors.length != LED_STRIP_EXT_BACK_COUNT) {
            throw new IllegalArgumentException("Invalid array sizes for ext LEDs");
        }
        
        LedStaticCommand cmd = new LedStaticCommand(target);
        cmd.extAvColors = extAvColors;
        cmd.extArColors = extArColors;
        return cmd;
    }
    
    // Helper pour créer une couleur uniforme sur un strip roof
    public static LedStaticCommand createUniformRoof(StaticTarget target, LedData color) {
        LedData[] roof1 = new LedData[LED_STRIP_1_COUNT];
        LedData[] roof2 = new LedData[LED_STRIP_2_COUNT];
        
        for (int i = 0; i < LED_STRIP_1_COUNT; i++) {
            roof1[i] = color;
        }
        for (int i = 0; i < LED_STRIP_2_COUNT; i++) {
            roof2[i] = color;
        }
        
        return createRoof(target, roof1, roof2);
    }
    
    // Helper pour créer une couleur uniforme sur un strip ext
    public static LedStaticCommand createUniformExt(StaticTarget target, LedData color) {
        LedData[] extAv = new LedData[LED_STRIP_EXT_FRONT_COUNT];
        LedData[] extAr = new LedData[LED_STRIP_EXT_BACK_COUNT];
        
        for (int i = 0; i < LED_STRIP_EXT_FRONT_COUNT; i++) {
            extAv[i] = color;
        }
        for (int i = 0; i < LED_STRIP_EXT_BACK_COUNT; i++) {
            extAr[i] = color;
        }
        
        return createExt(target, extAv, extAr);
    }
    
    public int getSize() {
        int size = 1; // target
        if (target.isRoof()) {
            size += LED_STRIP_1_COUNT * LedData.SIZE;
            size += LED_STRIP_2_COUNT * LedData.SIZE;
        } else {
            size += LED_STRIP_EXT_FRONT_COUNT * LedData.SIZE;
            size += LED_STRIP_EXT_BACK_COUNT * LedData.SIZE;
        }
        return size;
    }
    
    public void writeToBuffer(ByteBuffer buffer) {
        buffer.put((byte) target.getValue());
        
        if (target.isRoof()) {
            for (LedData led : roof1Colors) {
                led.writeToBuffer(buffer);
            }
            for (LedData led : roof2Colors) {
                led.writeToBuffer(buffer);
            }
        } else {
            for (LedData led : extAvColors) {
                led.writeToBuffer(buffer);
            }
            for (LedData led : extArColors) {
                led.writeToBuffer(buffer);
            }
        }
    }
    
    @Override
    public String toString() {
        return "LedStaticCommand{target=" + target + "}";
    }
    
    // Getters
    public StaticTarget getTarget() { return target; }
}
