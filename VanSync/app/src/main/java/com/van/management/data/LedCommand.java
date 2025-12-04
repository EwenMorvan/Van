package com.van.management.data;

import java.nio.ByteBuffer;

/**
 * Représentation Java de la structure led_command_t
 */
public class LedCommand {
    
    public enum LedType {
        STATIC(0),
        DYNAMIC(1);
        
        private final int value;
        LedType(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    private LedType ledType;
    private Object command; // LedStaticCommand ou LedDynamicCommand
    
    private LedCommand(LedType ledType, Object command) {
        this.ledType = ledType;
        this.command = command;
    }
    
    public static LedCommand createStatic(LedStaticCommand staticCommand) {
        return new LedCommand(LedType.STATIC, staticCommand);
    }
    
    public static LedCommand createDynamic(LedDynamicCommand dynamicCommand) {
        return new LedCommand(LedType.DYNAMIC, dynamicCommand);
    }
    
    public int getSize() {
        int size = 1; // led_type
        if (ledType == LedType.STATIC) {
            size += ((LedStaticCommand) command).getSize();
        } else {
            size += ((LedDynamicCommand) command).getSize();
        }
        return size;
    }
    
    public void writeToBuffer(ByteBuffer buffer) {
        buffer.put((byte) ledType.getValue());
        
        if (ledType == LedType.STATIC) {
            ((LedStaticCommand) command).writeToBuffer(buffer);
        } else {
            ((LedDynamicCommand) command).writeToBuffer(buffer);
        }
    }
    
    @Override
    public String toString() {
        return "LedCommand{" +
                "ledType=" + ledType +
                ", command=" + command +
                '}';
    }
    
    // Getters
    public LedType getLedType() { return ledType; }
    public Object getCommand() { return command; }
}
