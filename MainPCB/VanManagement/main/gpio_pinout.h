#ifndef GPIO_PINOUT_H
#define GPIO_PINOUT_H

#include "driver/gpio.h"

// LED Strip GPIO pins
#define DI_LED1             4    // Roof LED Strip 1
#define DI_LED2             5    // Roof LED Strip 2
#define DI_LED_AV           6    // Exterior LED Strip Front
#define DI_LED_AR           7    // Exterior LED Strip Back
#define EXT_LED             42    // Exterior LED Power Control

// MPPT UART pins (multiplexed on UART1)
#define VE_DIRECT_TX0       UART_PIN_NO_CHANGE    // 100|50 MPPT TX (RX only, no TX needed)
#define VE_DIRECT_RX0       18    // 100|50 MPPT RX
#define VE_DIRECT_TX1       UART_PIN_NO_CHANGE    // 70|15 MPPT TX (RX only, no TX needed)
#define VE_DIRECT_RX1       15    // 70|15 MPPT RX

// Sensor UART pins (multiplexed on UART2)
#define HEATER_TX           8   // Diesel Heater TX (RX only)
#define HCO2T_TX            3   // Humidity/CO2/Temperature/Light sensor TX (RX only)

// USB OTG pins
#define USB_A9_N            19   // USB OTG D-
#define USB_A9_D            20   // USB OTG D+

// Analog sensors
#define TEMP_ONBOARD        9   // Onboard temperature sensor (ADC)
#define FUEL_GAUGE          14   // Fuel gauge (ADC)

// W5500 Ethernet SPI pins
#define SPI_CS              10   // Chip Select
#define SPI_MOSI            11   // MOSI
#define SPI_MISO            13   // MISO
#define SPI_CLK             12   // Clock
#define W5500_RST_1         2   // Reset

// Fan control pins
#define FAN_ELEC_BOX_PWM    21   // Electric box fan PWM
#define FAN_HEATER_PWM      47   // Heater radiator fans PWM
#define LED_FAN             48   // Fan RGB LEDs

// Pump and heater control
#define PH                  1   // Pump Output
#define HEATER_ON_SIG       39   // Heater ON signal

// Input pins
#define INTER               41   // Roof LED switches (combined)
#define VAN_LIGHT           40   // Old van light input (door state)

// Hood fan control
#define HOOD_FAN            38   // Hood fan on/off

// LED strip lengths (number of LEDs)
#define LED_STRIP_1_COUNT   120           // 2m strip at ~60 LEDs/m
#define LED_STRIP_2_COUNT   120           // 2m strip at ~60 LEDs/m
#define LED_STRIP_EXT_FRONT_COUNT  60     // 1m strip
#define LED_STRIP_EXT_BACK_COUNT   60     // Combined back strips

#endif // GPIO_PINOUT_H