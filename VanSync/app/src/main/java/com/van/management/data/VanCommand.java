package com.van.management.data;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Représentation Java de la structure van_command_t du protocole C
 * Permet de construire et sérialiser des commandes pour l'ESP32
 */
public class VanCommand {
    
    // Command types (correspond à command_type_t)
    public enum CommandType {
        LED(0),
        HEATER(1),
        HOOD(2),
        WATER_CASE(3),
        MULTIMEDIA(4);
        
        private final int value;
        CommandType(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    private CommandType type;
    private long timestamp;
    private Object commandData;
    
    private VanCommand(CommandType type, Object commandData) {
        this.type = type;
        this.timestamp = System.currentTimeMillis();
        this.commandData = commandData;
    }
    
    // Factory methods pour créer différents types de commandes
    
    public static VanCommand createLedCommand(LedCommand ledCommand) {
        return new VanCommand(CommandType.LED, ledCommand);
    }
    
    public static VanCommand createHeaterCommand(HeaterCommand heaterCommand) {
        return new VanCommand(CommandType.HEATER, heaterCommand);
    }
    
    public static VanCommand createHoodCommand(HoodCommand hoodCommand) {
        return new VanCommand(CommandType.HOOD, hoodCommand);
    }
    
    public static VanCommand createWaterCaseCommand(WaterCaseCommand waterCaseCommand) {
        return new VanCommand(CommandType.WATER_CASE, waterCaseCommand);
    }
    
    public static VanCommand createProjectorCommand(ProjectorCommand projectorCommand) {
        return new VanCommand(CommandType.MULTIMEDIA, projectorCommand);
    }
    
    /**
     * Sérialise la commande en tableau de bytes selon le protocole C
     * Format: [type(1)] [timestamp(4)] [command_data(variable)]
     */
    public byte[] toBytes() {
        // Calculer la taille totale
        int totalSize = 1 + 4; // type + timestamp
        
        switch (type) {
            case LED:
                totalSize += ((LedCommand) commandData).getSize();
                break;
            case HEATER:
                totalSize += HeaterCommand.SIZE;
                break;
            case HOOD:
                totalSize += 1;
                break;
            case WATER_CASE:
                totalSize += 1;
                break;
            case MULTIMEDIA:
                totalSize += 1; // ProjectorCommand est juste 1 byte
                break;
        }
        
        ByteBuffer buffer = ByteBuffer.allocate(totalSize);
        buffer.order(ByteOrder.LITTLE_ENDIAN); // ESP32 est little-endian
        
        // Écrire le type de commande (1 byte)
        buffer.put((byte) type.getValue());
        
        // Écrire le timestamp (4 bytes)
        buffer.putInt((int) (timestamp & 0xFFFFFFFF));
        
        // Écrire les données spécifiques à la commande
        switch (type) {
            case LED:
                ((LedCommand) commandData).writeToBuffer(buffer);
                break;
            case HEATER:
                ((HeaterCommand) commandData).writeToBuffer(buffer);
                break;
            case HOOD:
                buffer.put((byte) ((HoodCommand) commandData).getValue());
                break;
            case WATER_CASE:
                buffer.put((byte) ((WaterCaseCommand) commandData).getCaseNumber().getValue());
                break;
            case MULTIMEDIA:
                buffer.put(((ProjectorCommand) commandData).toByte());
                break;
        }
        
        return buffer.array();
    }
    
    /**
     * Retourne une représentation hexadécimale des bytes pour le debug
     */
    public String toHexString() {
        byte[] bytes = toBytes();
        StringBuilder sb = new StringBuilder();
        sb.append("Command size: ").append(bytes.length).append(" bytes\n");
        sb.append("Hex dump:\n");
        
        for (int i = 0; i < bytes.length; i++) {
            if (i > 0 && i % 16 == 0) {
                sb.append("\n");
            }
            sb.append(String.format("%02x ", bytes[i] & 0xFF));
        }
        
        return sb.toString();
    }
    
    @Override
    public String toString() {
        return "VanCommand{" +
                "type=" + type +
                ", timestamp=" + timestamp +
                ", commandData=" + commandData +
                '}';
    }
    
    // Getters
    public CommandType getType() { return type; }
    public long getTimestamp() { return timestamp; }
    public Object getCommandData() { return commandData; }
}
