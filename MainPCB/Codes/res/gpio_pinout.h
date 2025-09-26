#ifndef GPIO_PINOUT_H
#define GPIO_PINOUT_H

#include "driver/gpio.h"

// LED Strip GPIO pins
#define DI_LED1             GPIO_NUM_1    // Roof LED Strip 1
#define DI_LED2             GPIO_NUM_2    // Roof LED Strip 2
#define DI_LED_AV           GPIO_NUM_3    // Exterior LED Strip Front
#define DI_LED_AR           GPIO_NUM_4    // Exterior LED Strip Back
#define EXT_LED             GPIO_NUM_5    // Exterior LED Power Control

// MPPT UART pins
#define VE_DIRECT_TX0       GPIO_NUM_6    // 100|50 MPPT TX
#define VE_DIRECT_RX0       GPIO_NUM_7    // 100|50 MPPT RX
#define VE_DIRECT_TX1       GPIO_NUM_8    // 70|15 MPPT TX
#define VE_DIRECT_RX1       GPIO_NUM_9    // 70|15 MPPT RX

// Sensor UART pins
#define HEATER_TX           GPIO_NUM_10   // Diesel Heater TX
#define HCO2T_TX            GPIO_NUM_11   // Humidity/CO2/Temperature/Light sensor TX

// USB OTG pins
#define USB_A9_N            GPIO_NUM_19   // USB OTG D-
#define USB_A9_D            GPIO_NUM_20   // USB OTG D+

// Analog sensors
#define TEMP_ONBOARD        GPIO_NUM_12   // Onboard temperature sensor (ADC)
#define FUEL_GAUGE          GPIO_NUM_13   // Fuel gauge (ADC)

// W5500 Ethernet SPI pins
#define SPI_CS              GPIO_NUM_14   // Chip Select
#define SPI_MOSI            GPIO_NUM_15   // MOSI
#define SPI_MISO            GPIO_NUM_16   // MISO
#define SPI_CLK             GPIO_NUM_17   // Clock
#define W5500_RST_1         GPIO_NUM_18   // Reset

// Fan control pins
#define FAN_ELEC_BOX_PWM    GPIO_NUM_21   // Electric box fan PWM
#define FAN_HEATER_PWM      GPIO_NUM_22   // Heater radiator fans PWM
#define LED_FAN             GPIO_NUM_23   // Fan RGB LEDs

// Pump and heater control
#define PH                  GPIO_NUM_24   // Pump Output
#define HEATER_ON_SIG       GPIO_NUM_25   // Heater ON signal

// Input pins
#define INTER               GPIO_NUM_26   // Roof LED switches (combined)
#define VAN_LIGHT           GPIO_NUM_27   // Old van light input (door state)

// Hood fan control
#define HOOD_FAN            GPIO_NUM_28   // Hood fan on/off

// LED strip lengths (number of LEDs)
#define LED_STRIP_1_COUNT   120           // 2m strip at ~60 LEDs/m
#define LED_STRIP_2_COUNT   120           // 2m strip at ~60 LEDs/m
#define LED_STRIP_EXT_FRONT_COUNT  60     // 1m strip
#define LED_STRIP_EXT_BACK_COUNT   60     // Combined back strips

#endif // GPIO_PINOUT_H