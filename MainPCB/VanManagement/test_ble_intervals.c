/**
 * Test program to analyze BLE notification intervals performance
 * This is a standalone analysis tool, not meant to be compiled into the main project
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// Simulated system parameters based on ESP32-S3 specs
#define CPU_FREQ_MHZ 160
#define FREERTOS_TICK_HZ 1000
#define JSON_PAYLOAD_SIZE 1217  // Current unformatted JSON size
#define BLE_MTU_DEFAULT 23      // Default BLE MTU (20 bytes data + 3 bytes header)
#define BLE_MTU_EXTENDED 244    // Extended MTU possible with negotiation

// Current task priorities and stack sizes (from your code)
#define COMM_TASK_STACK_SIZE 8192
#define COMM_TASK_PRIORITY 3

typedef struct {
    uint32_t interval_ms;
    uint32_t packets_per_second;
    uint32_t bytes_per_second;
    uint32_t cpu_cycles_per_second;
    float cpu_usage_percent;
    uint32_t memory_allocs_per_second;
} ble_performance_t;

// Estimated CPU cycles for different operations (calibrated for ESP32-S3)
#define CYCLES_JSON_GENERATION 50000    // JSON creation + serialization
#define CYCLES_BLE_TRANSMISSION 15000   // BLE stack processing
#define CYCLES_MEMORY_ALLOCATION 2000   // malloc/free overhead
#define CYCLES_TASK_SWITCHING 1000      // FreeRTOS task switch

void analyze_interval_performance(uint32_t interval_ms, ble_performance_t *perf) {
    perf->interval_ms = interval_ms;
    
    // Calculate transmission frequency
    perf->packets_per_second = 1000 / interval_ms;
    perf->bytes_per_second = (JSON_PAYLOAD_SIZE * 1000) / interval_ms;
    
    // Calculate CPU cycles needed per second
    uint32_t cycles_per_transmission = CYCLES_JSON_GENERATION + 
                                       CYCLES_BLE_TRANSMISSION + 
                                       CYCLES_MEMORY_ALLOCATION +
                                       CYCLES_TASK_SWITCHING;
    
    perf->cpu_cycles_per_second = cycles_per_transmission * perf->packets_per_second;
    
    // Calculate CPU usage percentage
    uint32_t total_cpu_cycles_per_second = CPU_FREQ_MHZ * 1000000;
    perf->cpu_usage_percent = ((float)perf->cpu_cycles_per_second / total_cpu_cycles_per_second) * 100.0f;
    
    // Memory allocations (JSON creation/destruction)
    perf->memory_allocs_per_second = perf->packets_per_second * 2; // alloc + free
}

void print_performance_analysis(ble_performance_t *perf) {
    printf("=== BLE Interval: %d ms ===\n", perf->interval_ms);
    printf("Transmissions per second: %d\n", perf->packets_per_second);
    printf("Data throughput: %d bytes/sec (%.2f KB/sec)\n", 
           perf->bytes_per_second, perf->bytes_per_second / 1024.0f);
    printf("Estimated CPU usage: %.2f%%\n", perf->cpu_usage_percent);
    printf("Memory allocations/sec: %d\n", perf->memory_allocs_per_second);
    
    // BLE packet analysis
    uint32_t packets_needed = (JSON_PAYLOAD_SIZE + BLE_MTU_DEFAULT - 4) / (BLE_MTU_DEFAULT - 3);
    printf("BLE packets needed (MTU %d): %d packets per transmission\n", 
           BLE_MTU_DEFAULT, packets_needed);
    
    uint32_t ble_packets_per_second = packets_needed * perf->packets_per_second;
    printf("Total BLE packets per second: %d\n", ble_packets_per_second);
    
    // Risk assessment
    printf("Risk Assessment: ");
    if (perf->cpu_usage_percent < 5.0f) {
        printf("LOW RISK - Excellent performance\n");
    } else if (perf->cpu_usage_percent < 15.0f) {
        printf("MEDIUM RISK - Good performance, monitor system\n");
    } else if (perf->cpu_usage_percent < 30.0f) {
        printf("HIGH RISK - May impact other tasks\n");
    } else {
        printf("CRITICAL RISK - System instability likely\n");
    }
    printf("\n");
}

int main() {
    printf("=== ESP32-S3 BLE Notification Interval Analysis ===\n");
    printf("JSON Payload Size: %d bytes\n", JSON_PAYLOAD_SIZE);
    printf("CPU Frequency: %d MHz\n", CPU_FREQ_MHZ);
    printf("FreeRTOS Tick Rate: %d Hz\n", FREERTOS_TICK_HZ);
    printf("\n");
    
    // Test different intervals
    uint32_t test_intervals[] = {2000, 1000, 500, 250, 100, 50, 25, 10};
    uint32_t num_tests = sizeof(test_intervals) / sizeof(test_intervals[0]);
    
    ble_performance_t results[8];
    
    for (int i = 0; i < num_tests; i++) {
        analyze_interval_performance(test_intervals[i], &results[i]);
        print_performance_analysis(&results[i]);
    }
    
    printf("=== RECOMMENDATIONS ===\n");
    printf("1. CONSERVATIVE (2000ms): Current setting, very safe\n");
    printf("2. BALANCED (500-1000ms): Good compromise for most use cases\n");
    printf("3. RESPONSIVE (100-250ms): Excellent UI responsiveness\n");
    printf("4. AGGRESSIVE (50ms): Maximum responsiveness, needs testing\n");
    printf("5. EXTREME (<50ms): Not recommended for production\n");
    printf("\n");
    
    printf("=== PRACTICAL CONSIDERATIONS ===\n");
    printf("- UART operations: Multiple UARTs running concurrently\n");
    printf("- MPPT data collection: Every 5 seconds\n");
    printf("- Sensor reading: Continuous ADC operations\n");
    printf("- LED management: PWM operations\n");
    printf("- Fan control: PWM + monitoring\n");
    printf("- BLE MTU: May need negotiation for larger MTU\n");
    printf("- Power consumption: Higher frequencies = more power\n");
    
    return 0;
}
