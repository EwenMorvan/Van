# Van Management System - Communication Protocol Documentation

## Overview

This document describes the communication protocol for the ESP32-S3 Van Management System, designed for Android app developers to integrate with the van's control system via Bluetooth Low Energy (BLE) and USB interfaces.

## Table of Contents

1. [Protocol Overview](#protocol-overview)
2. [Connection Types](#connection-types)
3. [Message Format](#message-format)
4. [State Notifications](#state-notifications)
5. [Commands](#commands)
6. [Error Handling](#error-handling)
7. [Android Implementation Guide](#android-implementation-guide)
8. [Code Examples](#code-examples)

---

## Protocol Overview

### Communication Methods
- **Primary**: Bluetooth Low Energy (BLE) - Fully implemented
- **Secondary**: USB OTG - Placeholder implementation

### Message Format
- **Encoding**: JSON (UTF-8)
- **Transport**: BLE GATT Characteristics or USB Serial
- **Maximum Message Size**: 2048 bytes (JSON), 512 bytes (BLE buffer)

### Update Intervals
- **Default**: 2000ms (2 seconds)
- **Configurable**: 100ms to 5000ms
- **Recommended for UI responsiveness**: 500ms to 1000ms

---

## Connection Types

### 1. Bluetooth Low Energy (BLE)

#### Device Information
- **Device Name**: `VanManagement`
- **Service UUID**: `0xAAA0` (16-bit)
- **Advertising**: Continuous when not connected
- **MTU**: 256 bytes (configurable up to 512 bytes)

#### GATT Service Structure
```
Van Management Service (0xAAA0)
├── Command Characteristic (0xAAA1)
│   ├── Properties: WRITE, WRITE_NO_RSP
│   ├── Permissions: WRITE
│   └── Purpose: Receive commands from app
└── State Characteristic (0xAAA2)
    ├── Properties: NOTIFY
    ├── Permissions: READ
    ├── Purpose: Send state updates to app
    └── Descriptor: Client Characteristic Configuration (0x2902)
```

#### Connection Parameters
- **Connection Interval**: 20ms - 40ms
- **Slave Latency**: 0
- **Supervision Timeout**: 4000ms
- **Max Connections**: 3 concurrent

### 2. USB Interface (Future Implementation)
- **Type**: USB OTG
- **Protocol**: Serial communication over USB
- **Baud Rate**: Not applicable (USB native speed)
- **Format**: Same JSON protocol as BLE

---

## Message Format

All messages use JSON format with the following structure:

### Base Message Structure
```json
{
  "type": "string",     // Message type: "state", "command", "response", "error"
  "timestamp": number,  // Unix timestamp in milliseconds
  "data": object       // Message-specific data (varies by type)
}
```

### Message Types
1. **`state`** - Periodic system state updates (ESP32 → App)
2. **`command`** - Control commands (App → ESP32)
3. **`response`** - Command acknowledgments (ESP32 → App)
4. **`error`** - Error notifications (ESP32 → App)

---

## State Notifications

The ESP32 periodically broadcasts the complete system state via BLE notifications. These are automatically sent every 2 seconds (configurable).

### State Message Structure
```json
{
  "type": "state",
  "timestamp": 1643723400000,
  "data": {
    "system": {
      "uptime": 123456,
      "system_error": false,
      "error_code": 0,
      "slave_pcb_connected": true
    },
    "sensors": {
      "fuel_level": 75.5,
      "onboard_temperature": 22.3,
      "cabin_temperature": 18.7,
      "humidity": 45.2,
      "co2_level": 420,
      "light_level": 1250,
      "van_light_active": false,
      "door_open": false
    },
    "heater": {
      "heater_on": true,
      "target_water_temp": 65.0,
      "target_cabin_temp": 20.0,
      "water_temperature": 62.3,
      "pump_active": true,
      "radiator_fan_speed": 75
    },
    "mppt": {
      "solar_power_100_50": 245.7,
      "battery_voltage_100_50": 13.2,
      "battery_current_100_50": 18.6,
      "temperature_100_50": 35,
      "state_100_50": 3,
      "solar_power_70_15": 89.3,
      "battery_voltage_70_15": 13.1,
      "battery_current_70_15": 6.8,
      "temperature_70_15": 32,
      "state_70_15": 2
    },
    "fans": {
      "elec_box_speed": 40,
      "heater_fan_speed": 60,
      "hood_fan_active": false
    },
    "leds": {
      "roof": {
        "current_mode": 2,
        "brightness": 80,
        "switch_pressed": false,
        "last_switch_time": 1643723350000
      },
      "exterior": {
        "current_mode": 1,
        "brightness": 100,
        "power_enabled": true
      },
      "error_mode_active": false
    }
  }
}
```

### Data Field Descriptions

#### System Data
- **`uptime`**: System uptime in milliseconds
- **`system_error`**: Boolean indicating if any system error is active
- **`error_code`**: Bitmask of active errors (see Error Codes section)
- **`slave_pcb_connected`**: Connection status to secondary PCB

#### Sensor Data
- **`fuel_level`**: Diesel fuel level percentage (0-100)
- **`onboard_temperature`**: PCB temperature in Celsius
- **`cabin_temperature`**: Interior cabin temperature in Celsius
- **`humidity`**: Relative humidity percentage (0-100)
- **`co2_level`**: CO2 concentration in ppm
- **`light_level`**: Ambient light level (0-4095)
- **`van_light_active`**: True if van interior lights are on
- **`door_open`**: True if van door is open

#### Heater Data
- **`heater_on`**: Webasto heater operational status
- **`target_water_temp`**: Desired water temperature (°C)
- **`target_cabin_temp`**: Desired cabin temperature (°C)
- **`water_temperature`**: Current water temperature (°C)
- **`pump_active`**: Water circulation pump status
- **`radiator_fan_speed`**: Radiator fan speed percentage (0-100)

#### MPPT Data (Two Controllers)
- **`solar_power_*`**: Solar panel power output in Watts
- **`battery_voltage_*`**: Battery voltage in Volts
- **`battery_current_*`**: Charging current in Amperes
- **`temperature_*`**: MPPT controller temperature in Celsius
- **`state_*`**: Charging state (0=Off, 1=Bulk, 2=Absorption, 3=Float, 4=Equalize)

#### Fan Data
- **`elec_box_speed`**: Electronics box cooling fan speed (0-100)
- **`heater_fan_speed`**: Heater circulation fan speed (0-100)
- **`hood_fan_active`**: Kitchen hood exhaust fan status

#### LED Data
- **`roof.current_mode`**: Active LED mode (0-4)
- **`roof.brightness`**: Brightness level (0-100)
- **`roof.switch_pressed`**: Physical switch state
- **`exterior.current_mode`**: Exterior lighting mode
- **`exterior.brightness`**: Exterior brightness level
- **`exterior.power_enabled`**: Master power enable for exterior lights

---

## Commands

Commands are sent from the Android app to the ESP32 to control various systems.

### Command Message Structure
```json
{
  "type": "command",
  "timestamp": 1643723400000,
  "cmd": "command_name",
  "target": 0,        // Optional: target device/component
  "value": 123        // Command-specific value
}
```

### Available Commands

#### 1. Heater Control

**Set Heater State**
```json
{
  "type": "command",
  "cmd": "set_heater_state",
  "target": 0,
  "value": 1    // 0=Off, 1=On
}
```

**Set Water Temperature Target**
```json
{
  "type": "command",
  "cmd": "set_heater_target",
  "target": 0,    // 0=Water, 1=Cabin
  "value": 65     // Temperature in Celsius
}
```

#### 2. LED Control

**Set LED Mode**
```json
{
  "type": "command",
  "cmd": "set_led_mode",
  "target": 0,    // 0=Roof, 1=Exterior
  "value": 2      // Mode number (0-4)
}
```

**Set LED Brightness**
```json
{
  "type": "command",
  "cmd": "set_led_brightness",
  "target": 0,    // 0=Roof, 1=Exterior
  "value": 80     // Brightness percentage (0-100)
}
```

#### 3. Fan Control

**Set Fan Speed**
```json
{
  "type": "command",
  "cmd": "set_fan_speed",
  "target": 0,    // 0=ElecBox, 1=Heater, 2=Hood
  "value": 60     // Speed percentage (0-100)
}
```

### Command Responses

Every command receives a response:

**Success Response**
```json
{
  "type": "response",
  "timestamp": 1643723400000,
  "status": "ok",
  "message": "Command executed"
}
```

**Error Response**
```json
{
  "type": "response",
  "timestamp": 1643723400000,
  "status": "error",
  "message": "Invalid command format"
}
```

---

## Error Handling

### System Error Codes (Bitmask)
```javascript
const ERROR_CODES = {
  NONE: 0x00,              // No errors
  HEATER_NO_FUEL: 0x01,    // Bit 0: Heater out of fuel
  MPPT_COMM: 0x02,         // Bit 1: MPPT communication error
  SENSOR_COMM: 0x04,       // Bit 2: Sensor communication error
  SLAVE_COMM: 0x08,        // Bit 3: Secondary PCB communication error
  LED_STRIP: 0x10,         // Bit 4: LED strip error
  FAN_CONTROL: 0x20        // Bit 5: Fan control error
};
```

### Error Message Format
```json
{
  "type": "error",
  "timestamp": 1643723400000,
  "error_code": 2,
  "message": "MPPT communication timeout"
}
```

### BLE Connection Issues
- **Connection Lost**: Monitor BLE connection state
- **MTU Negotiation**: Request larger MTU for better performance
- **Notification Failure**: Check if notifications are properly enabled
- **Write Timeout**: Implement retry logic for commands

---

## Android Implementation Guide

### 1. BLE Connection Setup

```kotlin
// Scan for van management device
private fun startBleScan() {
    val scanFilter = ScanFilter.Builder()
        .setDeviceName("VanManagement")
        .build()
    
    bluetoothLeScanner.startScan(
        listOf(scanFilter),
        scanSettings,
        scanCallback
    )
}

// Connect to device
private fun connectToDevice(device: BluetoothDevice) {
    bluetoothGatt = device.connectGatt(
        context,
        false,
        gattCallback,
        BluetoothDevice.TRANSPORT_LE
    )
}
```

### 2. Service Discovery

```kotlin
private val gattCallback = object : BluetoothGattCallback() {
    override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
        if (status == BluetoothGatt.GATT_SUCCESS) {
            val service = gatt.getService(UUID.fromString("0000AAA0-0000-1000-8000-00805F9B34FB"))
            commandCharacteristic = service.getCharacteristic(UUID.fromString("0000AAA1-0000-1000-8000-00805F9B34FB"))
            stateCharacteristic = service.getCharacteristic(UUID.fromString("0000AAA2-0000-1000-8000-00805F9B34FB"))
            
            // Enable notifications
            gatt.setCharacteristicNotification(stateCharacteristic, true)
            val descriptor = stateCharacteristic.getDescriptor(
                UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")
            )
            descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            gatt.writeDescriptor(descriptor)
        }
    }
}
```

### 3. Message Handling

```kotlin
// Parse incoming state notifications
private fun handleStateNotification(data: ByteArray) {
    val json = String(data, Charsets.UTF_8)
    val gson = Gson()
    val stateMessage = gson.fromJson(json, StateMessage::class.java)
    
    // Update UI with new state
    updateVanState(stateMessage.data)
}

// Send commands
private fun sendCommand(command: String, target: Int, value: Int) {
    val commandMessage = CommandMessage(
        type = "command",
        timestamp = System.currentTimeMillis(),
        cmd = command,
        target = target,
        value = value
    )
    
    val json = gson.toJson(commandMessage)
    commandCharacteristic.value = json.toByteArray(Charsets.UTF_8)
    bluetoothGatt.writeCharacteristic(commandCharacteristic)
}
```

### 4. Data Classes

```kotlin
data class StateMessage(
    val type: String,
    val timestamp: Long,
    val data: VanState
)

data class VanState(
    val system: SystemData,
    val sensors: SensorData,
    val heater: HeaterData,
    val mppt: MpptData,
    val fans: FanData,
    val leds: LedData
)

data class SystemData(
    val uptime: Long,
    val system_error: Boolean,
    val error_code: Int,
    val slave_pcb_connected: Boolean
)

// ... other data classes for each subsystem
```

### 5. MTU Optimization

```kotlin
override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
    if (newState == BluetoothProfile.STATE_CONNECTED) {
        // Request larger MTU for better performance
        gatt.requestMtu(512)
    }
}

override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
    if (status == BluetoothGatt.GATT_SUCCESS) {
        Log.d(TAG, "MTU changed to: $mtu")
        // Now discover services
        gatt.discoverServices()
    }
}
```

---

## Code Examples

### JavaScript/TypeScript (React Native)

```typescript
interface VanState {
  system: {
    uptime: number;
    system_error: boolean;
    error_code: number;
    slave_pcb_connected: boolean;
  };
  sensors: {
    fuel_level: number;
    cabin_temperature: number;
    humidity: number;
    // ... other sensor fields
  };
  // ... other subsystems
}

class VanBleManager {
  private device: Device | null = null;
  private stateCharacteristic: Characteristic | null = null;
  private commandCharacteristic: Characteristic | null = null;

  async connect(): Promise<void> {
    // Scan and connect to device
    this.device = await BleManager.connectToDevice(deviceId);
    await this.device.discoverAllServicesAndCharacteristics();
    
    // Get characteristics
    const service = await this.device.services();
    const vanService = service.find(s => s.uuid === "0000AAA0-0000-1000-8000-00805F9B34FB");
    
    this.stateCharacteristic = await vanService.characteristic("0000AAA2-0000-1000-8000-00805F9B34FB");
    this.commandCharacteristic = await vanService.characteristic("0000AAA1-0000-1000-8000-00805F9B34FB");
    
    // Monitor state notifications
    this.stateCharacteristic.monitor((error, characteristic) => {
      if (characteristic?.value) {
        const json = base64.decode(characteristic.value);
        const state: VanState = JSON.parse(json);
        this.onStateUpdate(state);
      }
    });
  }

  async sendCommand(cmd: string, target: number, value: number): Promise<void> {
    const command = {
      type: "command",
      timestamp: Date.now(),
      cmd,
      target,
      value
    };
    
    const json = JSON.stringify(command);
    const base64Data = base64.encode(json);
    
    await this.commandCharacteristic?.writeWithResponse(base64Data);
  }

  private onStateUpdate(state: VanState): void {
    // Update React state or emit event
    console.log("Van state updated:", state);
  }
}
```

### Flutter/Dart

```dart
class VanBleService {
  static const String SERVICE_UUID = "0000AAA0-0000-1000-8000-00805F9B34FB";
  static const String COMMAND_CHAR_UUID = "0000AAA1-0000-1000-8000-00805F9B34FB";
  static const String STATE_CHAR_UUID = "0000AAA2-0000-1000-8000-00805F9B34FB";

  BluetoothDevice? _device;
  BluetoothCharacteristic? _commandChar;
  BluetoothCharacteristic? _stateChar;

  Future<void> connect() async {
    // Scan for device
    FlutterBluePlus.instance.scan().listen((scanResult) {
      if (scanResult.device.name == "VanManagement") {
        _device = scanResult.device;
        _connectToDevice();
      }
    });
  }

  Future<void> _connectToDevice() async {
    await _device!.connect();
    
    List<BluetoothService> services = await _device!.discoverServices();
    BluetoothService vanService = services.firstWhere(
      (service) => service.uuid.toString().toUpperCase() == SERVICE_UUID
    );

    _commandChar = vanService.characteristics.firstWhere(
      (char) => char.uuid.toString().toUpperCase() == COMMAND_CHAR_UUID
    );
    
    _stateChar = vanService.characteristics.firstWhere(
      (char) => char.uuid.toString().toUpperCase() == STATE_CHAR_UUID
    );

    // Enable notifications
    await _stateChar!.setNotifyValue(true);
    _stateChar!.value.listen((value) {
      String json = utf8.decode(value);
      Map<String, dynamic> state = jsonDecode(json);
      _onStateUpdate(state);
    });
  }

  Future<void> sendCommand(String cmd, int target, int value) async {
    Map<String, dynamic> command = {
      'type': 'command',
      'timestamp': DateTime.now().millisecondsSinceEpoch,
      'cmd': cmd,
      'target': target,
      'value': value,
    };

    String json = jsonEncode(command);
    List<int> bytes = utf8.encode(json);
    await _commandChar!.write(bytes);
  }

  void _onStateUpdate(Map<String, dynamic> state) {
    // Handle state update
    print('Van state updated: $state');
  }
}
```

---

## Protocol Versions and Compatibility

### Current Version: 1.0
- Initial implementation with BLE support
- JSON-based messaging
- 16-bit UUIDs for simplicity
- 2-second default notification interval

### Planned Features (v1.1)
- USB OTG communication
- Compressed JSON or binary protocol option
- Dynamic notification intervals
- Enhanced error reporting
- Firmware update capability over BLE

---

## Performance Considerations

### Notification Intervals
- **2000ms (Default)**: Optimal for battery life, adequate for monitoring
- **1000ms**: Good balance of responsiveness and efficiency
- **500ms**: High responsiveness for active control
- **100ms**: Maximum responsiveness, higher power consumption

### Message Size Optimization
- Current JSON payload: ~1217 bytes unformatted
- BLE MTU: 256 bytes (requires 5-6 packets per notification)
- Consider enabling MTU negotiation to 512 bytes for better efficiency

### Connection Parameters
- Use connection interval 20-40ms for responsive control
- Implement proper reconnection logic for BLE reliability
- Handle Android BLE limitations (concurrent connections, scanning)

---

## Troubleshooting

### Common Issues

1. **Connection Failures**
   - Ensure Bluetooth permissions are granted
   - Check device is advertising and not already connected
   - Verify UUID formats (16-bit UUIDs need proper expansion)

2. **Large Message Handling**
   - JSON messages may be split across multiple BLE packets
   - Implement message reassembly for fragmented notifications
   - Consider increasing MTU size

3. **Command Timeouts**
   - Implement retry logic with exponential backoff
   - Check write characteristic permissions
   - Verify command JSON format

4. **State Update Issues**
   - Ensure notifications are properly enabled
   - Check descriptor write success
   - Monitor connection state changes

### Debug Information
- Enable ESP32 logging to see BLE events and command processing
- Use Android BLE debugging tools
- Monitor MTU negotiation and connection parameters
- Check JSON parsing errors on both sides

---

## Contact and Support

For technical questions or issues with this protocol:
- Check ESP32 logs for detailed error information
- Verify BLE service and characteristic UUIDs
- Ensure proper JSON formatting for commands
- Test with a BLE debugging app first

This protocol is designed to be simple, reliable, and efficient for controlling and monitoring van systems from Android applications.
