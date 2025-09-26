The MainPCB is the circuit placed on the side shelf in the electronics compartment. It is responsible for managing the entire van. Since there were not enough GPIO pins available and because some components are located on the opposite side of the van, the MainPCB delegates part of the work to a SlavePCB, and they communicate together.

The MainPCB is connected to multiple devices:
- 2 SK6812 LED Strips on the roof:
    - LED Strip 1: (DI_LED1)
    - LED Strip 2: (DI_LED2)
- 3 SK6812 LED Strips at the exterior:
    - LED Strip Ext front: (DI_LED_AV)
    - LED Strip Ext back: (DI_LED_AR)
- Due to potential interference in the DI pin or the possibility that the addressable LEDs retain their last state if powered even without a signal, for road safety, all exterior LED power is controlled by a pin:
    - Exterior LED power control: (EXT_LED)
- 2 MPPTs for the solar panels through UART:
    - 100|50 MPPT TX: (VE_DIRECT_TX0)
    - 100|50 MPPT RX: (VE_DIRECT_RX0)
    - 70|15 MPPT TX: (VE_DIRECT_TX1)
    - 70|15 MPPT RX: (VE_DIRECT_RX1)
- 2 UART RX receivers to read data from sensors/heater:
    - Diesel Heater TX: (HEATER_TX)
    - Humidity/CO2/Temperature/Light sensor TX: (HCO2T_TX)
- 1 USB OTG connection for a fixed Android tablet:
    - (USB_A9_N) and (USB_A9_D)
- 1 onboard analog temperature sensor: (TEMP_ONBOARD)
- 1 W5500 for Ethernet communication with the SlavePCB through SPI:
    - Chip Select: (SPI_CS)
    - MOSI: (SPI_MOSI)
    - MISO: (SPI_MISO)
    - Clock: (SPI_CLK)
    - Reset: (W5500_RST_1)
- 1 analog fuel gauge for the heater: (FUEL_GAUGE)
- 1 fan in the electric box in case all power devices (MPPTs, Battery, Inverter, etc.) overheat:
    - PWM pin: (FAN_ELEC_BOX_PWM)
- 4 fans (combined together) in the heater radiator to blow heat from the hot fluid:
    - PWM pin: (FAN_HEATER_PWM)
- Each fan has some RGB LEDs inside, not very useful but combined together and assigned to a pin:
    - Fan LED pin: (LED_FAN)
- 1 pump used to circulate the hot fluid in the heater radiator:
    - Pump Output: (PH)
- 2 switches to easily control LEDs on the roof:
    - Both switches combined together: (INTER)
- 1 logic input connected to the old default light on the roof. These lights were powered on for a certain time if a door was opened. By connecting an input to these cables, it is possible to use the signal to turn on the roof LEDs:
    - Input pin: (VAN_LIGHT)
    - TODO: Add TVS or Zener for protection.

- 1 output signal to power on or off the heater: (HEATER_ON_SIG)
- 1 output signal to power on the hood fan.

ALL THE COMPONENTS/SENSORS have the correct circuitry on the PCB and just need digital/analog/specific signals to work.

The goal is to create an ESP32-IDF project in the folder "Codes," well-structured and functional with error management.

The project should contain all GPIO assignments in a file named `gpio_pinout.h` in the `res` folder.

The project should have multiple files and tasks:
- Communication Manager
- Heater Manager
- MPPT Manager
- LED Manager
- USB Manager
- BLE Manager
- Fan Manager
- Sensor Manager
- Main

## Communication Manager:

This task should manage all communication between other tasks and the communication with the SlavePCB through Ethernet.

A clear protocol object should be created to reflect the whole state of any device/sensor in the van. This object should be sent every time to the Android app, and if the Android app or any of the managers changes something, sends data, or requests a change, the object should be updated only after confirmation that the change did not cause an error.

## Heater Manager:
This task manages the heater, such as the Heater ON signal pin, the data received through RX, and the fluid pump. It sends orders to the Communication Manager to power on the radiator fan.

This task listens for heater instructions given by the Communication Manager, like (heater on, target water temperature, target van temperature).

It turns on the heater until the water temperature is below the target and turns on the radiator fan until the van temperature is below the target and if the water temperature is above a threshold. It would be nice to implement PID regulation.

To turn on the heater, just set `HEATER_ON_SIG` to 1 and to 0 to turn it off. The water temperature is obtained through the RX UART of `HEATER_TX`. The van temperature is obtained by listening to the Communication Manager.

It is also extremely important to listen to the Communication Manager for the fuel level and not turn on the heater if the fuel is empty.

## Sensor Manager
This task is responsible for reading all sensors, such as:
- Fuel sensor
- Onboard temperature sensor
- Humidity sensor
- Van cabin temperature sensor
- CO2 sensor
- Light sensor
- Old van light state (for door state)

The fuel sensor and the onboard temperature sensor are analog sensors, but the Humidity/Temperature/CO2/Light sensor is an IC sensor located on the roof. Due to the lack of cables between the MainPCB and the roof, it is impossible to pass IC data directly, so a small microcontroller is added to convert the I2C to a UART one-direction signal. Simply read the UART data to retrieve values from Humidity/Temperature/CO2/Light.

It should also read the state of the van light.

Then the task periodically publishes the sensor values to the Communication Manager.

## MPPT Manager
This task is responsible for reading the information from the 2 MPPTs.

For now, it simply reads from 2 UART ports (maybe later some writing), then parses the messages following the protocol defined in https://www.victronenergy.com/upload/documents/VE.Direct-Protocol-3.34.pdf. It only retrieves relevant data like states, solar power, temperatures, etc., and maybe other data depending on the protocol.

These relevant data need to be sent to the Communication Manager.

## USB Manager and BLE Manager
Context: I will create an Android app that can be used on different devices via USB or BLE. The app should run simultaneously on all devices, and any change in the system state or action taken by one device should be reflected in real-time on other devices. The app will just be a UI with buttons to send commands and elements to visualize information; all logic will reside in the ESP32.

The USB Manager and BLE Manager are responsible for connecting to Android devices through USB if a device is found and via BLE to other phones. They read any incoming data from all Android devices and send it to the Communication Manager. Conversely, they should also read any data from the Communication Manager and send it to the apps.

A clear, customizable, and portable protocol should be used to transfer all this data clearly.

## Fan Manager
The Fan Manager is responsible for managing all fans, as not all fans are simple digital outputs; some require PWM.

This manager should contain functions like `start_fan(fan)` and `stop_fan(fan)` and will be called by reading the Communication Manager if a fan needs to be activated.

- For the heater fan, they are classical PC 120mm fans and need PWM (possible speed variation).
- For the fan in the electric box, it is also a PWM fan.
- For the hood fan, it is just an on/off output.

Every fan state should be sent to the Communication Manager at periodic intervals.

## LED Manager
The LED Manager is responsible for managing every light on the van.
ALL LEDs are SK6812 RGBWW chips.

- For roof LED strips:
    - Roof LEDs consist of 2 strips of 2m each.

    - It should have a listener on the switch state (off 0, on 1, already pulled down).
    The switch listener should be used to implement some predefined functions by detecting short clicks and long presses.
    Every short click will be used to change modes, and a long press will be used to change brightness. By default, 1 short click should go to the default state, which is natural white at full power. By default, a long press will lower the brightness until the minimum (not off), and if the user continues pressing the button, the brightness should increase to the maximum and repeat until the user releases the switch.

    - To add other modes, I will add functionality in the Android app UI to define up to 5 custom cases where the user can select colors/brightness/which LEDs/animations. Once added, the modes will be saved in the ESP32 memory and will be launched either by a button on the app (a message) or by multiple short clicks on the switch. For example:
    Mode 1 = Default = 1 short click
    Mode 2 = Custom 1 = 2 short clicks
    Mode 3 = Custom 2 = 3 short clicks
    Ect...
    

    - Remember, in any mode, a long press will loop over the brightness until the switch is released.

    - With the exact same UI on the app as for setting modes, I want the app to control LEDs in real-time, and this mode will be added to a custom mode only if saved.

    - The LEDs will also have different modes triggered not by the switch but by events and conditions, like door open, system command error, etc.

    - Door_open mode: For example, if I open the door (e.g., van_light high) and the light sensor indicates low ambient luminosity -> go to predefined mode "door open" and play its animation. But if the light sensor indicates it is already bright, do not trigger the case. Do not use the state of the light sensor after turning off the LEDs; it would be illogical.
            - Animation: Time 4s (simultaneously on both LED strips). Set luminosity to 25%, starting by turning on the last LED and making a "dot moving" animation by moving the light to the first one, passing across all others. Then let the first LED stay on, re-turn on the last LED, and move the dot to the 2nd LED, etc., until the strips are fully on.
    Once the van_light signal turns low after a certain time, play the animation in reverse but over 20s to give the user time to understand that the light will turn off. If the user is still in the van, they will trigger the switch.

    - Error mode:
        For now, just flash one LED out of two in red 5 times to indicate an error and prompt the user to check the Android app.

- For exterior LED strips:
    - Same as roof LEDs, but there is no physical switch, so switches are on the app. Since the switches are on the app, there is no need to manage short, long, or multiple clicks. The app will allow direct changes to custom modes. Also, custom modes for exterior LEDs are different from those for roof LEDs.

    - Exterior LEDs consist of one lateral strip of 1m and 2 small strips at the back, but they act as one because DI pins are soldered together.
