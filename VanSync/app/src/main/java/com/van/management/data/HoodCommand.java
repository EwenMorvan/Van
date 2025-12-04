package com.van.management.data;

/**
 * Représentation Java de l'enum hood_command_t
 */
public enum HoodCommand {
    OFF(0),
    ON(1);
    
    private final int value;
    
    HoodCommand(int value) {
        this.value = value;
    }
    
    public int getValue() {
        return value;
    }
    
    public static HoodCommand fromValue(int value) {
        for (HoodCommand cmd : values()) {
            if (cmd.value == value) {
                return cmd;
            }
        }
        throw new IllegalArgumentException("Invalid hood command value: " + value);
    }
}
