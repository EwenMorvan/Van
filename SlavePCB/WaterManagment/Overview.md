# SlavePCB Overview

The SlavePCB is responsible for managing the following components:
- The electrovalves and pumps for the kitchen and shower.
- The buttons for controlling the kitchen and shower.
- The load cells located under the water tanks.
- Communication with the MainPCB via Ethernet (W5500) to receive instructions and send sensor data.

## Code Structure

The code is organized into four main sections:

1. [Electrovalves and Pump Manager](#electrovalves-and-pump-manager)
2. [Buttons Manager](#buttons-manager)
3. [Load Cell Manager](#load-cell-manager)
4. [Communication Manager](#communication-manager)

---

## Electrovalves and Pump Manager

This section handles the control and operation of the electrovalves and the pumps for the kitchen, shower and purge.

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
```
signal (on/off), directing the selected input to the output.

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



### Code behaviour

The Electrovalve and Pump Manager listens to the Communication Manager for the current system case (default is RST). Based on the received case, it adjusts the electrovalves and pumps according to the logic table. Before activating the pumps, it ensures all electrovalves have reached their desired positions. 

To verify this, it uses the `is_electrovalve_turning(electrovalve)` function, which reads the current through the electrovalve via I2C (using a TCA I2C multiplexer to address the correct bus). If the current is below a threshold, the function returns `false`, indicating the electrovalve has stopped turning.

Once the electrovalve has stopped turning. It send the info to the communication manager.

For pumps, it uses a similar function, `is_pump_pumping_water(pump)`, to check if the pump is operating correctly. If the PV pump fails to pump water (e.g., due to lack of priming or an empty dirty water tank), it sends a notification to the Communication Manager ("PV pump isn't pumping water"). For the PE pump, if no current is detected for a certain interval, the system switches to the RST case and notifies the Communication Manager of the change.

This ensures pumps are only activated when the electrovalves are in the correct state and provides feedback for error handling.

## Buttons Manager

This section manages the input from the buttons used to control the kitchen and shower.

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


### Code behaviour

The Button Manager runs continuously, monitoring the state of each button. For physical buttons, it uses the `read_button_click(btn)` function to detect short or long clicks. For virtual buttons (e.g., on a tablet), it listens to the Communication Manager for button states sent via the W5500 Ethernet module. The manager stores the virtual state of each button.

Based on the button states, it determines the corresponding system case using the button-case table and checks for incompatible cases. Once a case is determined, it updates the button LEDs using the `set_buttons_leds(case)` function, which applies the LED color logic. It also sends the updated case state to the Communication Manager.

When the code initiates a case change, it should activate all LEDs in yellow (red + green) except for the BH LED. It then listens to the Communication Manager to determine when the electrovalves have finished turning before applying the appropriate LED colors for the selected case.

Additionally, the Button Manager listens for RST messages from the Communication Manager. When an RST message is received, it resets the stored button states and updates the LEDs accordingly. This ensures the system remains synchronized and responsive to both physical and virtual inputs.

## Load Cell Manager

This section processes data from the load cells located under the water tanks.

Under eatch Water tank there is 4 load cell that allow to measure the weight of the tank and so the water volume. Sine tehre is 5 water tank, eatch load cell of a tank are cabled to one HX711 to reduce number of cables. So there is 5 HX711 connected to the PCB. The Clock (SCK) line of all of these HX711 is shared and the ESP32 is connected to the 5 DT lines.

### Code behaviour

The Load Cell Manager continuously reads data from the load cells under the water tanks. Each tank has four load cells connected to an HX711 module, with five HX711 modules in total. The SCK (clock) line is shared among all HX711 modules, while the ESP32 is connected to their individual DT lines.

The manager reads each HX711 module sequentially, ensuring proper handling of the shared SCK line to avoid conflicts. It periodically sends the collected weight data to the Communication Manager, allowing the system to monitor water levels in real-time. This ensures accurate and reliable water volume measurements for all tanks.

## Communication Manager

This section manages Ethernet communication (W5500) with the MainPCB to exchange instructions and sensor data.

### Code behaviour

The Communication Manager acts as the central hub for all inter-task communication. It facilitates data exchange between the Electrovalve and Pump Manager, Button Manager, and Load Cell Manager. It also manages Ethernet communication with the MainPCB via the W5500 module.

The manager listens for incoming instructions from the MainPCB and relays them to the appropriate task. It also collects data from the other managers (e.g., button states, load cell readings, pump/electrovalve statuses) and sends this information to the MainPCB. This ensures seamless coordination between all components and reliable communication with the main system.

## Water Tanck

A = 1 = recycled water 1
B = 2 = dirty water 1
C = 3 = recycled water 2
D = 4 = dirty water 2
E = Clean water