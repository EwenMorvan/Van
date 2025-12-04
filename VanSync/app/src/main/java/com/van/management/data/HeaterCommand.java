package com.van.management.data;

import java.nio.ByteBuffer;

/**
 * Représentation Java de la structure heater_command_t
 */
public class HeaterCommand {
    public static final int SIZE = 2 + 4 + 4 + 1; // 2 bools + 2 floats + 1 byte = 11 bytes
    
    private boolean heaterEnabled;
    private boolean radiatorPumpEnabled;
    private float waterTargetTemp;
    private float airTargetTemp;
    private int radiatorFanSpeed; // 0-255
    
    public HeaterCommand(boolean heaterEnabled, boolean radiatorPumpEnabled,
                        float waterTargetTemp, float airTargetTemp, int radiatorFanSpeed) {
        this.heaterEnabled = heaterEnabled;
        this.radiatorPumpEnabled = radiatorPumpEnabled;
        this.waterTargetTemp = Math.max(0, Math.min(100, waterTargetTemp));
        this.airTargetTemp = Math.max(0, Math.min(50, airTargetTemp));
        this.radiatorFanSpeed = Math.max(0, Math.min(255, radiatorFanSpeed));
    }
    
    // Constructeurs simplifiés
    public static HeaterCommand turnOff() {
        return new HeaterCommand(false, false, 0, 0, 0);
    }
    
    public static HeaterCommand turnOn(float waterTemp, float airTemp) {
        return new HeaterCommand(true, true, waterTemp, airTemp, 128);
    }
    
    public void writeToBuffer(ByteBuffer buffer) {
        buffer.put((byte) (heaterEnabled ? 1 : 0));
        buffer.put((byte) (radiatorPumpEnabled ? 1 : 0));
        buffer.putFloat(waterTargetTemp);
        buffer.putFloat(airTargetTemp);
        buffer.put((byte) radiatorFanSpeed);
    }
    
    @Override
    public String toString() {
        return "HeaterCommand{" +
                "heaterEnabled=" + heaterEnabled +
                ", radiatorPumpEnabled=" + radiatorPumpEnabled +
                ", waterTargetTemp=" + waterTargetTemp +
                ", airTargetTemp=" + airTargetTemp +
                ", radiatorFanSpeed=" + radiatorFanSpeed +
                '}';
    }
    
    // Getters et Setters
    public boolean isHeaterEnabled() { return heaterEnabled; }
    public void setHeaterEnabled(boolean enabled) { this.heaterEnabled = enabled; }
    
    public boolean isRadiatorPumpEnabled() { return radiatorPumpEnabled; }
    public void setRadiatorPumpEnabled(boolean enabled) { this.radiatorPumpEnabled = enabled; }
    
    public float getWaterTargetTemp() { return waterTargetTemp; }
    public void setWaterTargetTemp(float temp) { this.waterTargetTemp = Math.max(0, Math.min(100, temp)); }
    
    public float getAirTargetTemp() { return airTargetTemp; }
    public void setAirTargetTemp(float temp) { this.airTargetTemp = Math.max(0, Math.min(50, temp)); }
    
    public int getRadiatorFanSpeed() { return radiatorFanSpeed; }
    public void setRadiatorFanSpeed(int speed) { this.radiatorFanSpeed = Math.max(0, Math.min(255, speed)); }
}
