### Naming Description

- **EP**: Eau Propre
- **ER**: Eau Recyclée
- **ES**: Eau Sale
- **V**: Vidange
- **PE**: Pompe Eau
- **PD**: Pompe Douche
- **PV**: Pompe Vidange
- **PP**: Pompe Pluie


### Schematic for Electrovanne Type T

```plaintext
Input 1
|   |
|   |
|   ------
|          Input 2
|   ------
|   |
|   |
Output
signal (on/off), directing the selected input to the output.
```
- **Input 1**: The primary source of the fluid.
- **Input 2**: The secondary source of the fluid.
- **Output**: The destination of the fluid when the electrovanne is active.
- **Control**: The electrovanne switches between Input 1 and Input 2 based on its control 

### Schematic for Electrovanne Simple

```plaintext
Input
|   |
|   |
|   |
|   |
|   |
|   |
|   |
Output
```
- **Input**: The source of the fluid.
- **Output**: The destination of the fluid when the electrovanne is active.
- **Control**: The electrovanne switches Input ON or OFF based on its control 



### Electrovanne States and Descriptions

| Type        | State | Description     |
|-------------|-------|-----------------|
| T EV        |   0   | IN1 -> OUT      |
| T EV        |   1   | IN2 -> OUT      |
| Simple EV   |   0   | Fluid Blocked   |
| Simple EV   |   1   | IN -> OUT       |

### Cases Descriptions

| Cas   |       description       |
|-------|-------------------------|
|Evier  |                         |
| E1    | Evier EP -> ES          |
| E2    | Evier EP -> ER          |
| E3    | Evier ER -> ES          |
| E4    | Evier ER -> ER          |
|Douche |                         |
| D1    | Douche EP -> ES         |
| D2    | Douche EP -> ER         |
| D3    | Douche ER -> ES         |
| D4    | Douche ER -> ER         |
|Vidange|                         |
| V1    | Vidange ES -> V         |
| V2    | Vidange ER -> V         |
|Pluie  |                         |
| P1    | Recup Pluie -> ER       |

- RST = Reset Case All closed and pumps Off, this case should be triggered if
    - syteme boot
    - no change for 5 minutes
    - before systeme shutdown
    - if asked by the user

### State to check before switching cases

- **CE**: Clean Water empty
- **DF**: Dirt Water Full
- **DE**: Dirt Water Empty
- **RF**: Recycled Water Full
- **RE**: Recycled Water Empty


| State | Imcompatible cases |
|-------|--------------------|
|   CE  |     E1/E2/D1/D2    |
|   DF  |     E1/E3/D1/D3    |
|   DE  |         V1         |
|   RF  |E2/E3/E4/D2/D3/D4/P1|
|   RE  |   E3/E4/D3/D4/V2   |


### Van Plumber schematic

Todo Put a image 


### Electrovanne and pump logic table:

| Cas | A | B | C | D | E | PE | PD | PV | PP |
|-----|---|---|---|---|---|----|----|----|----| 
| RST | 0 | 0 | 0 | 0 | 0 |  0 |  0 |  0 |  0 | 
| E1  | 0 | 1 | 1 | 0 | 0 |  1 |  0 |  0 |  0 | 
| E2  | 0 | 1 | 0 | 0 | 0 |  1 |  0 |  0 |  0 | 
| E3  | 1 | 1 | 1 | 0 | 0 |  1 |  0 |  0 |  0 | 
| E4  | 1 | 1 | 0 | 0 | 0 |  1 |  0 |  0 |  0 | 
| D1  | 0 | 0 | 1 | 1 | 0 |  1 |  1 |  0 |  0 | 
| D2  | 0 | 0 | 0 | 1 | 0 |  1 |  1 |  0 |  0 | 
| D3  | 1 | 0 | 1 | 1 | 0 |  1 |  1 |  0 |  0 | 
| D4  | 1 | 0 | 0 | 1 | 0 |  1 |  1 |  0 |  0 | 
| V1  | 0 | 0 | 1 | 0 | 1 |  0 |  0 |  1 |  0 | 
| V2  | 0 | 0 | 0 | 0 | 1 |  0 |  0 |  1 |  0 | 
| P1  |0-1| 0 | 0 | 0 | 1 |  0 |  0 |  0 |  1 |





### Button code

| Button Type       | Button ID | Description                                   |
|-------------------|-----------|-----------------------------------------------|
| Physical/Digital Buttons  | BE1       | Activates water flow to the sink (Evier).     |
|                   | BE2       | Toggles recycling for the sink (Evier).       |
|                   | BD1       | Activates water flow to the shower (Douche).  |
|                   | BD2       | Toggles recycling for the shower (Douche).    |
|                   | BH        | Toggles hotte.                                |
| Digital Buttons   | BV1       | Triggers drainage of dirty water (Eau Sale).  |
|                   | BV2       | Triggers drainage of recycled water (Eau Recyclée). |
|                   | BP1       | Triggers collection of rainwater (Eau de Pluie). |
|                   | BRST      | Trigger the reset configuration and stop all pump |



### Physical Button color code
| Case | BE1 | BE2 | BD1 | BD2 |
|------|-----|-----|-----|-----|
|  E1  |Green| Red | Off | Off |
|  E2  |Green|Green| Off | Off |
|  E3  | Red | Red | Off | Off |
|  E4  | Red |Green| Off | Off |
|  D1  | Off | Off |Green| Red |
|  D2  | Off | Off |Green|Green|
|  D3  | Off | Off | Red | Red |
|  D4  | Off | Off | Red |Green|
|  V1  | Off | Off | Off | Off |
|  V2  | Off | Off | Off | Off |
|  P1  | Off | Off | Off | Off |
|  RST | Off | Off | Off | Off |

- Yellow (Red+Green) = Changing Cases Please wait (time taken by the electrovalves to mecanicly turn)


### Digital Button logic
| Case | BV1 | BV2 | BP1 | BRST|
|------|-----|-----|-----|-----|
|  E1  | Off | Off | Off | Off |
|  E2  | Off | Off | Off | Off |
|  E3  | Off | Off | Off | Off |
|  E4  | Off | Off | Off | Off |
|  D1  | Off | Off | Off | Off |
|  D2  | Off | Off | Off | Off |
|  D3  | Off | Off | Off | Off |
|  D4  | Off | Off | Off | Off |
|  V1  | On  | Off | Off | Off |
|  V2  | Off | On  | Off | Off |
|  P1  | Off | Off | On  | Off |
|  RST | Off | Off | Off | On  |



#### Imcompatible Cases Button Color Code

The buttons will blink in the color of the correponding case. And no Pump can be activated
- For exemple if BE1 is set to Green (Clean water to the sink) and the state CE is true (Clean water empty) the BE1 Button will blink in Green.



### Physical Button click logic

- **Short Click (SC)**:
    - **BE1/BD1**: Toggle between clean (Green, E1/D1) and recycled water (Red, E3/D3) or initiate E/D cases from RST.
    - **BE2/BD2**: Toggle recycling state within E/D cases (e.g., E1 ↔ E2, D1 ↔ D2).
    - **BV1/BV2/BP1/BRST**: Directly trigger V1, V2, P1, or RST.
- **Long Click (LC)**:
    - Only BE1/BD1 can trigger RST; other buttons’ long clicks are ignored or maintain current case.

Priority: Newest button click takes precedence.


## Electronics Section

### Electrovalves and pump

The electrovalves are controlled using GPIO signals, which toggle between states 0 and 1 as defined in the case table. These signals are duplicated, inverted, and amplified using BJT transistors, and then driven by channel-P high-side MOSFETs.

The 12V input is boosted to 20.4V using an MT3608 boost converter. This higher voltage reduces the switching time for the electrovalves, improving performance (e.g., 14 seconds at 20.4V compared to 23 seconds at 12V).

The 20.4V supply is distributed to each electrovalve via INA219 I2C current sensors. These sensors monitor the current consumed by each electrovalve, allowing the system to determine whether the valves are actively switching. This is particularly useful during case transitions to confirm when the transition phase is complete.

Since there are five electrovalves and the INA219 sensors only support four I2C addresses, an I2C multiplexer (TCA9548A) is used to expand the available I2C bus addresses.

The four water pumps (PE, PD, PV, PP) are also controlled via GPIO signals and MOSFET amplifiers. A signal of 0 turns the pump off, while a signal of 1 activates the pump. Additionally, two INA219 sensors are used to monitor the current for PE and PV pumps:

- **PE Pump**: Equipped with a built-in pressure sensor switch. The pump automatically stops when the pressure in the circuit exceeds a certain threshold (e.g., when the sink is closed) and starts again when the pressure drops (e.g., when the sink is open). The current sensor allows the system to detect when the pump is active, which is also useful for monitoring the PD pump. The PD pump extracts water from the shower due to its lower height compared to the water tank. If the user closes the shower faucet, the PE pump stops automatically, meaning there should be no water in the shower tray, making work the PD pump unnecessary in this scenario.

- **PV Pump**: Positioned farther from the water tank, its performance can be affected by the van's inclination. Since the pump is not self-priming, it may fail to drain water if the tank is not properly aligned. The current sensor helps detect whether the pump is pumping air (low current) or water (higher current), allowing the system to inform the user to adjust the van's inclination for optimal drainage.

### HX711 Management

Each HX711 module requires two GPIO pins for communication. Since there are five water tanks, a total of 10 GPIO pins are needed to interface with the HX711 modules.

### Button and LED Management

The system includes five buttons, each requiring one GPIO pin with pull-down resistors. Additionally:
- Four buttons are equipped with two LEDs each.
- One button has a single LED.
This results in a total of nine GPIO pins required for LED control.

### Power Regulation

The system receives two power inputs: 12V and 5V. Since the ESP32 operates at 3.3V, a buck converter (LM2596) is used to step down the 5V input to 3.3V.

### Communication Between ESP32 Modules

Communication between ESP32 modules is established using the SPI protocol over a 2-meter RJ45 cable.

