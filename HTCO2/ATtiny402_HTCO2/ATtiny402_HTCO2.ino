// ==============================================================================
// ATtiny402 SCD41 CO2 Sensor Reader
// ==============================================================================
// Sketch uses 3275 bytes (79%) of program storage space. Maximum is 4096 bytes. 
//
// HARDWARE SETUP:
// - ATtiny402 with SCD41 CO2 sensor via I2C
// - Photoresistor on PIN_PA7
// - LED indicator on PIN_PA3 (optional)
//
// PROGRAMMING THE ATtiny402 (UPDI):
// ----------------------------------
// Hardware needed:
//   - USB-to-UART converter (or Arduino/ESP32 with reset button held during upload)
//
// Wiring for UPDI programming:
//   ATtiny402 PA0 (UPDI) --> 1kΩ resistor --> TX of USB-UART (or RX of Arduino/ESP)
//   ATtiny402 PA0 (UPDI) --> RX of USB-UART (or TX of Arduino/ESP)
//   ATtiny402 GND --> GND
//   ATtiny402 VDD --> 3.3V or 5V
//
// Software setup:
//   1. Install pymcuprog: pip install pymcuprog
//   2. Compile sketch in Arduino IDE (Sketch > Export Compiled Binary)
//   3. Navigate to sketch folder containing the .hex file
//
// Flash command (replace COMx with your port):
//   pymcuprog write -d attiny402 -t uart -u COMx -f ATtiny402_HTCO2.ino.hex --erase --verify
//
// READING SERIAL OUTPUT:
// ----------------------
// Connect USB-UART RX to ATtiny402 PA6 (TX) to read sensor data
// Baud rate: 115200
// Output format: CO2,Temperature_tenths,Humidity_tenths,Light
// Example: 1124,188,596,243 = 1124ppm, 18.8°C, 59. 6%RH, light=243
// ==============================================================================

#include <Wire.h>

#define SCD41_ADDR 0x62
#define PHOTORESISTOR_PIN PIN_PA7
#define LED_PIN PIN_PA3
#define MEASUREMENT_INTERVAL 5000

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(LED_PIN, OUTPUT);
  
  // Start SCD41 periodic measurement (command 0x21b1 from datasheet page 9)
  Wire.beginTransmission(SCD41_ADDR);
  Wire.write(0x21); 
  Wire.write(0xb1);
  Wire.endTransmission();
  
  // Wait for first measurement to be ready (5 seconds minimum)
  delay(5000);
}

void loop() {
  // Read measurement (command 0xec05 from datasheet page 9)
  Wire.beginTransmission(SCD41_ADDR);
  Wire.write(0xec); 
  Wire. write(0x05);
  Wire.endTransmission();
  
  delay(1); // Wait 1ms for command execution
  
  // Request 9 bytes: CO2(2) + CRC(1) + Temp(2) + CRC(1) + Hum(2) + CRC(1)
  Wire.requestFrom(SCD41_ADDR, 9);
  
  if (Wire.available() >= 9) {
    // Read CO2 (2 bytes + CRC)
    uint16_t co2 = Wire.read() << 8 | Wire.read();
    Wire. read(); // Skip CRC
    
    // Read temperature raw value (2 bytes + CRC)
    uint16_t temp_raw = Wire.read() << 8 | Wire.read();
    Wire.read(); // Skip CRC
    
    // Read humidity raw value (2 bytes + CRC)
    uint16_t hum_raw = Wire.read() << 8 | Wire.read();
    Wire.read(); // Skip CRC
    
    // CO2 in ppm (parts per million)
    Serial.print(co2);
    Serial. write(',');
    
    // Temperature in tenths of °C: (-450 + temp_raw * 175 / 655. 35)
    // Formula: T = -45 + 175 * (temp_raw / 65535)
    // Multiply by 10 to get tenths: T*10 = -450 + 1750 * temp_raw / 65535
    // Simplified: T*10 = -450 + temp_raw * 1750 / 65535 ≈ -450 + temp_raw / 37. 45
    Serial.print((int16_t)(-450 + ((long)temp_raw * 1750L) / 65535L));
    Serial.write(',');
    
    // Humidity in tenths of %RH: (hum_raw * 100 / 65535) * 10
    // Simplified: hum_raw * 1000 / 65535
    Serial.print((uint16_t)(((long)hum_raw * 1000L) / 65535L));
    Serial.write(',');
    
    // Light sensor raw ADC value (0-1023)
    Serial.println(analogRead(PHOTORESISTOR_PIN));
    
    // Toggle LED to indicate successful reading
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  
  // Wait 5 seconds between measurements (signal update interval from datasheet)
  delay(MEASUREMENT_INTERVAL);
}