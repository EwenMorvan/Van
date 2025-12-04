package com.van.management.data;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Représentation Java de la commande vidéoprojecteur pour l'ESP32
 * Correspond à projector_command_t
 */
public class ProjectorCommand {
    
    // Command types (correspond à projector_command_t)
    public enum CommandType {
        DEPLOY(0),                  // Deploie completement (0x00)
        RETRACT(1),                 // Retracte completement (0x01)
        STOP(2),                    // Arrete le moteur (0x02)
        GET_STATUS(3),              // Demande le statut (0x03)
        JOG_UP_1(4),                // Avance de 1.0 tour (0x04)
        JOG_UP_01(5),               // Avance de 0.1 tour (0x05)
        JOG_UP_001(6),              // Avance de 0.01 tour (0x06)
        JOG_DOWN_1(7),              // Recule de 1.0 tour (0x07)
        JOG_DOWN_01(8),             // Recule de 0.1 tour (0x08)
        JOG_DOWN_001(9);            // Recule de 0.01 tour (0x09)
        
        private final int value;
        CommandType(int value) { this.value = value; }
        public int getValue() { return value; }
    }
    
    private CommandType command;
    
    public ProjectorCommand(CommandType command) {
        this.command = command;
    }
    
    // Factory methods
    public static ProjectorCommand deploy() {
        return new ProjectorCommand(CommandType.DEPLOY);
    }
    
    public static ProjectorCommand retract() {
        return new ProjectorCommand(CommandType.RETRACT);
    }
    
    public static ProjectorCommand stop() {
        return new ProjectorCommand(CommandType.STOP);
    }
    
    public static ProjectorCommand getStatus() {
        return new ProjectorCommand(CommandType.GET_STATUS);
    }
    
    public static ProjectorCommand jogUp1() {
        return new ProjectorCommand(CommandType.JOG_UP_1);
    }
    
    public static ProjectorCommand jogUp01() {
        return new ProjectorCommand(CommandType.JOG_UP_01);
    }
    
    public static ProjectorCommand jogUp001() {
        return new ProjectorCommand(CommandType.JOG_UP_001);
    }
    
    public static ProjectorCommand jogDown1() {
        return new ProjectorCommand(CommandType.JOG_DOWN_1);
    }
    
    public static ProjectorCommand jogDown01() {
        return new ProjectorCommand(CommandType.JOG_DOWN_01);
    }
    
    public static ProjectorCommand jogDown001() {
        return new ProjectorCommand(CommandType.JOG_DOWN_001);
    }
    
    /**
     * Sérialise la commande en byte (juste le code de commande)
     */
    public byte toByte() {
        return (byte) command.getValue();
    }
    
    /**
     * Retourne la représentation hexadécimale pour le debug
     */
    public String toHexString() {
        return String.format("ProjectorCommand: 0x%02x (%s)", command.getValue(), command.name());
    }
    
    @Override
    public String toString() {
        return "ProjectorCommand{" +
                "command=" + command +
                '}';
    }
    
    // Getters
    public CommandType getCommand() { return command; }
    public int getCommandValue() { return command.getValue(); }
}
