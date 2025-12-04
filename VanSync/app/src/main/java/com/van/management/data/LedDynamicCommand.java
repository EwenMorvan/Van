package com.van.management.data;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/**
 * Représentation Java de la structure led_dynamic_command_t
 * Commande dynamique pour les LEDs (Roof seulement, avec keyframes)
 */
public class LedDynamicCommand {
    
    // Cibles pour DYNAMIC (Roof seulement)
    public enum DynamicTarget {
        ROOF_LED1(0),
        ROOF_LED2(1),
        ROOF_LED_ALL(2);
        
        private final int value;
        DynamicTarget(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    // Comportement de boucle
    public enum LoopBehavior {
        ONCE(0),
        REPEAT(1),
        PING_PONG(2);
        
        private final int value;
        LoopBehavior(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    private DynamicTarget target;
    private long loopDurationMs;
    private LoopBehavior loopBehavior;
    private List<LedKeyframe> keyframes;
    
    public static final int MAX_KEYFRAMES = 100;
    
    private LedDynamicCommand(DynamicTarget target, long loopDurationMs, LoopBehavior loopBehavior) {
        this.target = target;
        this.loopDurationMs = loopDurationMs;
        this.loopBehavior = loopBehavior;
        this.keyframes = new ArrayList<>();
    }
    
    // Builder pattern pour faciliter la création
    public static class Builder {
        private DynamicTarget target;
        private long loopDurationMs;
        private LoopBehavior loopBehavior;
        private List<LedKeyframe> keyframes = new ArrayList<>();
        
        public Builder(DynamicTarget target, long loopDurationMs) {
            this.target = target;
            this.loopDurationMs = loopDurationMs;
            this.loopBehavior = LoopBehavior.REPEAT;
        }
        
        public Builder setLoopBehavior(LoopBehavior behavior) {
            this.loopBehavior = behavior;
            return this;
        }
        
        public Builder addKeyframe(LedKeyframe keyframe) {
            if (keyframes.size() >= MAX_KEYFRAMES) {
                throw new IllegalStateException("Maximum keyframes reached");
            }
            keyframes.add(keyframe);
            return this;
        }
        
        public LedDynamicCommand build() {
            if (keyframes.isEmpty()) {
                throw new IllegalStateException("At least one keyframe is required");
            }
            
            // Vérifier que les timestamps sont en ordre croissant
            for (int i = 1; i < keyframes.size(); i++) {
                if (keyframes.get(i).getTimestampMs() <= keyframes.get(i-1).getTimestampMs()) {
                    throw new IllegalStateException("Keyframe timestamps must be in ascending order");
                }
            }
            
            LedDynamicCommand cmd = new LedDynamicCommand(target, loopDurationMs, loopBehavior);
            cmd.keyframes = keyframes;
            return cmd;
        }
    }
    
    public int getSize() {
        int size = 1;  // target
        size += 4;     // loop_duration_ms
        size += 2;     // keyframe_count
        size += 1;     // loop_behavior
        
        for (LedKeyframe keyframe : keyframes) {
            size += keyframe.getSize(target);
        }
        
        return size;
    }
    
    public void writeToBuffer(ByteBuffer buffer) {
        buffer.put((byte) target.getValue());
        buffer.putInt((int) loopDurationMs);
        buffer.putShort((short) keyframes.size());
        buffer.put((byte) loopBehavior.getValue());
        
        for (LedKeyframe keyframe : keyframes) {
            keyframe.writeToBuffer(buffer, target);
        }
    }
    
    @Override
    public String toString() {
        return "LedDynamicCommand{" +
                "target=" + target +
                ", loopDurationMs=" + loopDurationMs +
                ", loopBehavior=" + loopBehavior +
                ", keyframeCount=" + keyframes.size() +
                '}';
    }
    
    // Getters
    public DynamicTarget getTarget() { return target; }
    public long getLoopDurationMs() { return loopDurationMs; }
    public LoopBehavior getLoopBehavior() { return loopBehavior; }
    public List<LedKeyframe> getKeyframes() { return keyframes; }
}
