# SlavePCB - ESP32-S3 Water Management System

## Overview

The WaterPCB is a comprehensive water management system built on ESP32-S3 using ESP-IDF framework. It manages electrovalves, pumps, buttons, load cells, and communicates with a MainPCB via Ethernet.

## Features

- **4 Manager System Architecture**:
  - Electrovalve and Pump Manager
  - Button Manager (physical and virtual buttons)
  - Load Cell Manager (HX711 integration)
  - Communication Manager (W5500 Ethernet)

- **Hardware Support**:
  - ESP32-S3 microcontroller
  - Shift register chains for output control
  - I2C current sensors with TCA multiplexer
  - HX711 load cell amplifiers (shared clock)
  - W5500 Ethernet controller
  - Physical buttons with LED feedback

- **Advanced Features**:
  - Comprehensive error management and logging
  - System health monitoring
  - Incompatible case detection
  - Real-time weight monitoring
  - Inter-task communication via FreeRTOS queues

## Architecture

### Core Components

1. **Main Coordinator**: Central logic and case management
2. **Shift Register Controller**: Generic output control system
3. **Error Manager**: Comprehensive error tracking and reporting
4. **System Monitor**: Health monitoring and diagnostics

### System Cases

The system supports 12 different operational cases:
- `RST`: Reset state (all off)
- `E1-E4`: Kitchen (Evier) water modes
- `D1-D4`: Shower (Douche) water modes  
- `V1-V2`: Drainage (Vidange) modes
- `P1`: Rain collection (Pluie) mode

### Device Types

- **Electrovalves**: A, B, C, D, E (Type T and Simple)
- **Pumps**: PE, PD, PV, PP
- **LEDs**: Button indicator LEDs (Red/Green)
- **Sensors**: Load cells, current sensors

## Hardware Configuration

### GPIO Pin Assignments

All GPIO assignments are defined in `res/gpio_pinout.h`:

**Digital Inputs:**
- Buttons: BE1(7), BE2(15), BD1(16), BD2(17), BH(18)
- HX711 DT lines: A(42), B(41), C(40), D(39), E(38)

**Digital Outputs:**
- Shift registers: MR(3), DS(14), STCP(21), SHCP(47), OE(48)
- I2C MUX control: A0(4), A1(5), A2(6)
- HX711 SCK(1), W5500 RST(2)

**Communication:**
- SPI (W5500): CS(10), MOSI(11), CLK(12), MISO(13)
- I2C (Current sensors): SDA(8), SCL(9)

### Shift Register Mapping

The system uses 4 cascaded shift registers (32 bits total):
- Register 0: Electrovalves A-E, Pumps PE-PD
- Register 1: Pump PV-PP, Button LEDs
- Registers 2-3: Additional LEDs and future expansion

## Software Architecture

### Task Structure

```
Priority 6: Main Coordinator (Case logic & error handling)
Priority 5: Communication Manager (Ethernet communication)  
Priority 4: Electrovalve/Pump Manager (Hardware control)
Priority 3: Button Manager (Input processing)
Priority 3: Load Cell Manager (Weight monitoring)
Priority 2: System Monitor (Health monitoring)
```

### Inter-Task Communication

- **comm_queue**: Main communication between all managers
- **button_queue**: Button events to communication manager
- **loadcell_queue**: Weight data to communication manager
- **output_mutex**: Shared resource protection for outputs

### Error Management

Comprehensive error tracking with categorization:
- I2C communication errors
- SPI communication errors  
- Device timeout errors
- Memory errors
- Incompatible case errors

## Configuration

### Build Configuration

Key configuration options in `sdkconfig.defaults`:
- ESP32-S3 target with 240MHz CPU
- Ethernet W5500 support
- Custom partition table
- FreeRTOS 1000Hz tick rate

### Runtime Configuration

Configurable parameters via menuconfig:
- Button debounce timing
- HX711 sampling parameters
- I2C frequency
- Network settings

## API Reference

### Core Functions

```c
// Generic output control
slave_pcb_err_t set_output_state(device_type_t device, bool state);

// Case compatibility checking
bool is_case_compatible(system_case_t case_id, uint32_t system_states);

// Error management
void log_system_error(slave_pcb_err_t error_code, const char* component, const char* description);

// System health
void check_system_health(void);
void print_system_status(void);
```

### Manager Functions

```c
// Manager initialization
slave_pcb_err_t electrovalve_pump_manager_init(void);
slave_pcb_err_t button_manager_init(void);
slave_pcb_err_t load_cell_manager_init(void);
slave_pcb_err_t communication_manager_init(void);
```

## Usage

### Building the Project

```bash
cd /path/to/SlavePCB/Codes
idf.py set-target esp32s3
idf.py menuconfig  # Optional: configure project settings
idf.py build
```

### Flashing

```bash
idf.py flash monitor
```

### Monitoring

The system provides comprehensive logging and monitoring:

```bash
# View real-time logs
idf.py monitor

# System status is printed every 30 seconds
# Error statistics available via logging
# Queue usage monitoring included
```

### Operation Modes

1. **Normal Operation**: System responds to button presses and MainPCB commands
2. **Error Recovery**: Automatic RST on critical failures
3. **Maintenance Mode**: All outputs can be controlled independently

## Safety Features

### Hardware Safety

- All outputs default to OFF state on startup
- Master reset capability for shift registers
- Output enable control for emergency shutdown
- Watchdog protection

### Software Safety

- Incompatible case detection prevents unsafe operations
- Mutex protection for shared resources
- Comprehensive error logging and recovery
- System health monitoring

### Operational Safety

- Water level monitoring prevents overflow/underflow
- Pump protection against dry running
- Automatic case switching on system errors
- Current monitoring for fault detection

## Troubleshooting

### Common Issues

1. **Shift Register Not Working**
   - Check wiring connections
   - Verify power supply
   - Check shift register timing

2. **I2C Communication Errors**
   - Verify pull-up resistors
   - Check multiplexer addressing
   - Confirm sensor power

3. **HX711 Not Responding**
   - Check shared clock line
   - Verify DT pin connections
   - Ensure proper power supply

4. **Ethernet Issues**
   - Check W5500 connections
   - Verify network configuration
   - Check cable and link status

### Debug Tools

- Real-time error counters
- System health status
- Shift register state display
- Queue usage monitoring

## Development

### Adding New Devices

1. Add device type to `device_type_t` enum
2. Update shift register mapping
3. Add case logic if needed
4. Update incompatibility matrix

### Extending Communication

1. Add message type to `msg_type_t`
2. Update communication protocol
3. Add message handlers

### Custom Error Handling

Use the error management system:
```c
log_system_error(SLAVE_PCB_ERR_CUSTOM, "COMPONENT", "Description");
```

