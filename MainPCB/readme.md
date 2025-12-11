# MainPCB Van Controller


## Features

- **LED Management**: Controls SK6812 RGBWW LED strips with custom modes and animations
- **Heater Control**: TODO
- **Solar Monitoring**: Reads data from two MPPT charge controllers via VE.Direct protocol
- **Sensor Management**: Monitors fuel, temperature, humidity, CO2, and light sensors
- **Fan Control**: PWM control for cooling fans and heater radiator fans
- **Communication**: Ethernet communication with SlavePCB, USB/BLE for mobile apps
- **Error Handling**: Comprehensive error detection and visual/audible notifications




### LED 
- Roof LED strips: 2 strips of 120 LEDs each (SK6812 RGBWW)
- Exterior LED strips: Front (60 LEDs) + Back (TODO)
- Switch-based mode selection with customizable animations
- Door-open detection with automatic lighting

### Heater 
TODO

### MPPT 
- Supports VE.Direct protocol from Victron Energy
- Monitors two charge controllers (100|50 and 70|15)
- Automatic error detection for communication failures

### Sensor
- Fuel gauge: Analog sensor (0-100%)
- Temperature sensors: Onboard and cabin
- Environmental sensors: Humidity, CO2, light level
- Door detection via van light signal

## Error Handling

The system implements comprehensive error handling:

- **Critical Errors**: Fuel empty, communication failures
- **Logging**: ESP-IDF logging system with configurable levels
- **Recovery**: Automatic retry and failsafe operations

## Communication Protocol

The system uses a structured protocol for communication:
- **Internal**: FreeRTOS queues between managers
- **External**: JSON-based protocol for mobile apps
- **SlavePCB**: Binary protocol over Ethernet


