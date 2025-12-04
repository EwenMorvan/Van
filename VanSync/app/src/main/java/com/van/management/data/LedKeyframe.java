package com.van.management.data;

import java.nio.ByteBuffer;

/**
 * Représentation Java de la structure led_keyframe_t
 * Un keyframe définit l'état des LEDs à un instant donné dans une animation
 */
public class LedKeyframe {
    
    // Mode de transition entre keyframes
    public enum TransitionMode {
        LINEAR(0),
        EASE_IN_OUT(1),
        STEP(2);
        
        private final int value;
        TransitionMode(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    private long timestampMs;
    private TransitionMode transition;
    private LedData[] roof1Colors;  // Pour ROOF_LED1 ou ROOF_LED_ALL
    private LedData[] roof2Colors;  // Pour ROOF_LED2 ou ROOF_LED_ALL
    
    private LedKeyframe(long timestampMs, TransitionMode transition) {
        this.timestampMs = timestampMs;
        this.transition = transition;
    }
    
    // Factory pour ROOF_LED1
    public static LedKeyframe createRoof1(long timestampMs, TransitionMode transition, LedData[] roof1Colors) {
        if (roof1Colors.length != LedStaticCommand.LED_STRIP_1_COUNT) {
            throw new IllegalArgumentException("Invalid array size for roof1 LEDs");
        }
        
        LedKeyframe keyframe = new LedKeyframe(timestampMs, transition);
        keyframe.roof1Colors = roof1Colors;
        return keyframe;
    }
    
    // Factory pour ROOF_LED2
    public static LedKeyframe createRoof2(long timestampMs, TransitionMode transition, LedData[] roof2Colors) {
        if (roof2Colors.length != LedStaticCommand.LED_STRIP_2_COUNT) {
            throw new IllegalArgumentException("Invalid array size for roof2 LEDs");
        }
        
        LedKeyframe keyframe = new LedKeyframe(timestampMs, transition);
        keyframe.roof2Colors = roof2Colors;
        return keyframe;
    }
    
    // Factory pour ROOF_LED_ALL
    public static LedKeyframe createRoofAll(long timestampMs, TransitionMode transition, 
                                           LedData[] roof1Colors, LedData[] roof2Colors) {
        if (roof1Colors.length != LedStaticCommand.LED_STRIP_1_COUNT || 
            roof2Colors.length != LedStaticCommand.LED_STRIP_2_COUNT) {
            throw new IllegalArgumentException("Invalid array sizes for roof LEDs");
        }
        
        LedKeyframe keyframe = new LedKeyframe(timestampMs, transition);
        keyframe.roof1Colors = roof1Colors;
        keyframe.roof2Colors = roof2Colors;
        return keyframe;
    }
    
    // Helper pour créer un keyframe avec une couleur uniforme
    public static LedKeyframe createUniform(long timestampMs, TransitionMode transition,
                                           LedDynamicCommand.DynamicTarget target, LedData color) {
        switch (target) {
            case ROOF_LED1: {
                LedData[] roof1 = new LedData[LedStaticCommand.LED_STRIP_1_COUNT];
                for (int i = 0; i < roof1.length; i++) roof1[i] = color;
                return createRoof1(timestampMs, transition, roof1);
            }
            case ROOF_LED2: {
                LedData[] roof2 = new LedData[LedStaticCommand.LED_STRIP_2_COUNT];
                for (int i = 0; i < roof2.length; i++) roof2[i] = color;
                return createRoof2(timestampMs, transition, roof2);
            }
            case ROOF_LED_ALL: {
                LedData[] roof1 = new LedData[LedStaticCommand.LED_STRIP_1_COUNT];
                LedData[] roof2 = new LedData[LedStaticCommand.LED_STRIP_2_COUNT];
                for (int i = 0; i < roof1.length; i++) roof1[i] = color;
                for (int i = 0; i < roof2.length; i++) roof2[i] = color;
                return createRoofAll(timestampMs, transition, roof1, roof2);
            }
            default:
                throw new IllegalArgumentException("Invalid target");
        }
    }
    
    public int getSize(LedDynamicCommand.DynamicTarget target) {
        int size = 4;  // timestamp_ms
        size += 1;     // transition
        
        switch (target) {
            case ROOF_LED1:
                size += LedStaticCommand.LED_STRIP_1_COUNT * LedData.SIZE;
                break;
            case ROOF_LED2:
                size += LedStaticCommand.LED_STRIP_2_COUNT * LedData.SIZE;
                break;
            case ROOF_LED_ALL:
                size += LedStaticCommand.LED_STRIP_1_COUNT * LedData.SIZE;
                size += LedStaticCommand.LED_STRIP_2_COUNT * LedData.SIZE;
                break;
        }
        
        return size;
    }
    
    public void writeToBuffer(ByteBuffer buffer, LedDynamicCommand.DynamicTarget target) {
        buffer.putInt((int) timestampMs);
        buffer.put((byte) transition.getValue());
        
        switch (target) {
            case ROOF_LED1:
                for (LedData led : roof1Colors) {
                    led.writeToBuffer(buffer);
                }
                break;
            case ROOF_LED2:
                for (LedData led : roof2Colors) {
                    led.writeToBuffer(buffer);
                }
                break;
            case ROOF_LED_ALL:
                for (LedData led : roof1Colors) {
                    led.writeToBuffer(buffer);
                }
                for (LedData led : roof2Colors) {
                    led.writeToBuffer(buffer);
                }
                break;
        }
    }
    
    @Override
    public String toString() {
        return "LedKeyframe{timestampMs=" + timestampMs + ", transition=" + transition + "}";
    }
    
    // Getters
    public long getTimestampMs() { return timestampMs; }
    public TransitionMode getTransition() { return transition; }
}
