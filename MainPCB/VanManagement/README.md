# MainPCB Van Controller

This is the ESP-IDF project for the MainPCB van controller system. The MainPCB manages all van electronics including lighting, heating, solar monitoring, and communication with mobile devices.

## Features

- **LED Management**: Controls SK6812 RGBWW LED strips with custom modes and animations
- **Heater Control**: PID-controlled diesel heater with water circulation pump
- **Solar Monitoring**: Reads data from two MPPT charge controllers via VE.Direct protocol
- **Sensor Management**: Monitors fuel, temperature, humidity, CO2, and light sensors
- **Fan Control**: PWM control for cooling fans and heater radiator fans
- **Communication**: Ethernet communication with SlavePCB, USB/BLE for mobile apps
- **Multi-App BLE Support**: Up to 3 mobile apps can connect simultaneously 📱📱📱
- **Error Handling**: Comprehensive error detection and visual/audible notifications

## Hardware Requirements

- ESP32-S3 microcontroller
- W5500 Ethernet module for SlavePCB communication
- Multiple UART interfaces for sensor communication
- PWM outputs for fan control
- Analog inputs for sensor readings
- GPIO outputs for heater and pump control

## Project Structure

```
Codes/
├── main/
│   ├── main.c                 # Main application entry point
│   ├── protocol.h/c           # Communication protocol definitions
│   ├── communication_manager.h/c  # Inter-component communication
│   ├── sensor_manager.h/c     # Sensor reading and processing
│   ├── led_manager.h/c        # LED strip control and animations
│   ├── heater_manager.h/c     # Heater and pump control with PID
│   ├── mppt_manager.h/c       # Solar charge controller monitoring
│   ├── fan_manager.h/c        # Cooling fan control
│   ├── usb_manager.h/c        # USB OTG communication
│   ├── ble_manager.h/c        # Bluetooth communication
│   └── w5500_ethernet.h/c     # Ethernet communication with SlavePCB
├── res/
│   └── gpio_pinout.h          # GPIO pin definitions
├── CMakeLists.txt             # Main CMake configuration
└── README.md                  # This file
```

## Building and Flashing

1. **Prerequisites**:
   - ESP-IDF v5.0 or later
   - Python 3.6+
   - CMake 3.16+

2. **Setup ESP-IDF Environment**:
   ```bash
   cd /path/to/esp-idf
   . ./export.sh
   ```

3. **Configure the Project**:
   ```bash
   cd /home/ewen/Documents/van/MainPCB/Codes
   idf.py set-target esp32s3
   idf.py menuconfig
   ```

4. **Build the Project**:
   ```bash
   idf.py build
   ```

5. **Flash to ESP32-S3**:
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

## Configuration

### LED Configuration
- Roof LED strips: 2 strips of 120 LEDs each (SK6812 RGBWW)
- Exterior LED strips: Front (60 LEDs) + Back (60 LEDs)
- Switch-based mode selection with customizable animations
- Door-open detection with automatic lighting

### Heater Configuration
- PID temperature control with configurable parameters
- Safety interlocks (fuel level, temperature limits)
- Automatic pump and fan control

### MPPT Configuration
- Supports VE.Direct protocol from Victron Energy
- Monitors two charge controllers (100|50 and 70|15)
- Automatic error detection for communication failures

### Sensor Configuration
- Fuel gauge: Analog sensor (0-100%)
- Temperature sensors: Onboard and cabin
- Environmental sensors: Humidity, CO2, light level
- Door detection via van light signal

## Error Handling

The system implements comprehensive error handling:

- **Critical Errors**: Fuel empty, communication failures
- **Visual Indicators**: LED error animations
- **Logging**: ESP-IDF logging system with configurable levels
- **Recovery**: Automatic retry and failsafe operations

## Communication Protocol

The system uses a structured protocol for communication:
- **Internal**: FreeRTOS queues between managers
- **External**: JSON-based protocol for mobile apps
- **SlavePCB**: Binary protocol over Ethernet

## Safety Features

- Fuel level monitoring with heater interlock
- Temperature monitoring and protection
- Fan failure detection
- Communication timeout handling
- Graceful degradation on component failures

## Customization

### Adding LED Modes
1. Modify `led_manager.c` to add new animation functions
2. Update the mode selection logic in the switch handler
3. Add corresponding UI elements in the mobile app

### Adjusting PID Parameters
Modify the constants in `heater_manager.h`:
```c
#define HEATER_PID_KP 2.0f
#define HEATER_PID_KI 0.1f
#define HEATER_PID_KD 0.5f
```

### Sensor Calibration
Update the conversion functions in `sensor_manager.c` based on your specific sensor characteristics.

## Troubleshooting

### Common Issues

1. **LED strips not working**:
   - Check power supply to exterior LEDs (EXT_LED pin)
   - Verify data line connections
   - Check LED strip count definitions

2. **MPPT communication failure**:
   - Verify UART baud rate (19200)
   - Check cable connections
   - Monitor with oscilloscope for data activity

3. **Heater not responding**:
   - Check fuel level sensor
   - Verify heater control signal
   - Monitor UART communication from heater

### Debug Output
Enable debug logging by setting log level in menuconfig:
```
Component config → Log output → Default log verbosity → Debug
```

## Multi-App BLE Support 📱

The system supports **up to 3 mobile apps** connecting simultaneously to the ESP32-S3.

**Features:**
- ✅ All apps receive real-time van state updates
- ✅ Commands from multiple apps are handled (first command has priority)
- ✅ Automatic reconnection and connection management
- ✅ Up to 4 BLE connections total (3 apps + 1 external device like battery)
