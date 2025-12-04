package com.van.management.data;

/**
 * Représentation Java de la structure water_case_command_t
 */
public class WaterCaseCommand {
    
    // Correspond à system_case_t dans votre code C
    public enum SystemCase {
        CASE_RST(0),
        CASE_1(1),
        CASE_2(2),
        CASE_3(3),
        CASE_4(4),
        CASE_5(5),
        CASE_6(6),
        CASE_7(7),
        CASE_8(8),
        CASE_MAX(9);
        
        private final int value;
        
        SystemCase(int value) {
            this.value = value;
        }
        
        public int getValue() {
            return value;
        }
        
        public static SystemCase fromValue(int value) {
            for (SystemCase c : values()) {
                if (c.value == value) {
                    return c;
                }
            }
            throw new IllegalArgumentException("Invalid system case value: " + value);
        }
    }
    
    private SystemCase caseNumber;
    
    public WaterCaseCommand(SystemCase caseNumber) {
        this.caseNumber = caseNumber;
    }
    
    @Override
    public String toString() {
        return "WaterCaseCommand{caseNumber=" + caseNumber + "}";
    }
    
    // Getters
    public SystemCase getCaseNumber() {
        return caseNumber;
    }
    
    public void setCaseNumber(SystemCase caseNumber) {
        this.caseNumber = caseNumber;
    }
}
