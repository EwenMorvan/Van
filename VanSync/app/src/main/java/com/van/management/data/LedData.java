package com.van.management.data;

import java.nio.ByteBuffer;

/**
 * Représentation Java de la structure led_data_t
 * Contient les valeurs RGBW et la luminosité pour une LED
 */
public class LedData {
    public static final int SIZE = 5; // r, g, b, w, brightness (5 bytes)
    
    private int r;
    private int g;
    private int b;
    private int w;
    private int brightness;
    
    public LedData(int r, int g, int b, int w, int brightness) {
        this.r = clamp(r);
        this.g = clamp(g);
        this.b = clamp(b);
        this.w = clamp(w);
        this.brightness = clamp(brightness);
    }
    
    // Constructeur simplifié pour RGB seulement
    public LedData(int r, int g, int b, int brightness) {
        this(r, g, b, 0, brightness);
    }
    
    // Constructeur pour blanc uniquement
    public static LedData white(int brightness) {
        return new LedData(0, 0, 0, 255, brightness);
    }
    
    // Constructeur pour éteindre
    public static LedData off() {
        return new LedData(0, 0, 0, 0, 0);
    }
    
    private int clamp(int value) {
        return Math.max(0, Math.min(255, value));
    }
    
    public void writeToBuffer(ByteBuffer buffer) {
        buffer.put((byte) r);
        buffer.put((byte) g);
        buffer.put((byte) b);
        buffer.put((byte) w);
        buffer.put((byte) brightness);
    }
    
    @Override
    public String toString() {
        return String.format("LED(R:%d G:%d B:%d W:%d Br:%d)", r, g, b, w, brightness);
    }
    
    // Getters
    public int getR() { return r; }
    public int getG() { return g; }
    public int getB() { return b; }
    public int getW() { return w; }
    public int getBrightness() { return brightness; }
}
