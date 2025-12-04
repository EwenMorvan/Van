package com.van.management.utils;

public class ErrorCodes {
    public static final int NONE = 0x00;
    public static final int HEATER_NO_FUEL = 0x01;
    public static final int MPPT_COMM = 0x02;
    public static final int SENSOR_COMM = 0x04;
    public static final int SLAVE_COMM = 0x08;
    public static final int LED_STRIP = 0x10;
    public static final int FAN_CONTROL = 0x20;
    
    public static String getErrorString(int errorCode) {
        StringBuilder errors = new StringBuilder();
        
        if ((errorCode & HEATER_NO_FUEL) != 0) {
            errors.append("Chauffage sans carburant, ");
        }
        if ((errorCode & MPPT_COMM) != 0) {
            errors.append("Erreur communication MPPT, ");
        }
        if ((errorCode & SENSOR_COMM) != 0) {
            errors.append("Erreur communication capteurs, ");
        }
        if ((errorCode & SLAVE_COMM) != 0) {
            errors.append("Erreur communication PCB secondaire, ");
        }
        if ((errorCode & LED_STRIP) != 0) {
            errors.append("Erreur bande LED, ");
        }
        if ((errorCode & FAN_CONTROL) != 0) {
            errors.append("Erreur contrôle ventilateurs, ");
        }
        
        if (errors.length() > 0) {
            errors.setLength(errors.length() - 2); // Enlever la dernière virgule
            return errors.toString();
        }
        
        return "Aucune erreur";
    }
}
