# Van Management System - Architecture Globale

## Table des Matières
1. [Vue d'ensemble du système](#vue-densemble-du-système)
2. [Architecture de Communication](#architecture-de-communication)
3. [Gestion d'État Centralisée](#gestion-détat-centralisée)
4. [Protocole de Communication](#protocole-de-communication)
5. [Global Coordinator](#global-coordinator)
6. [Modules Périphériques](#peripheral-devices-module-documentation)
7. [Gestion des Erreurs](#gestion-des-erreurs)
8. [Ajout de Nouveaux Composants](#ajout-de-nouveaux-composants)

---

## Vue d'ensemble du système

### Architecture en 3 Couches

```
┌─────────────────────────────────────────────────────────────┐
│                     APPLICATION MOBILE                       │
│                    (Flutter via BLE)                         │
└─────────────────────┬───────────────────────────────────────┘
                      │ BLE (JSON)
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                   MAIN PCB (ESP32-S3)                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         COMMUNICATION PROTOCOL LAYER                  │   │
│  │  • comm_protocol.c/h                                  │   │
│  │  • Gère les commandes BLE ↔ Actions                  │   │
│  │  • Formate l'état global en JSON                     │   │
│  └─────────┬────────────────────────────┬────────────────┘   │
│            │                            │                    │
│  ┌─────────▼──────────┐      ┌──────────▼────────────┐      │
│  │ GLOBAL COORDINATOR  │      │   STATE AGGREGATOR    │      │
│  │ • Gestion événements│      │ • van_state_t         │      │
│  │ • Pub/Sub système   │      │ • battery_state_t     │      │
│  │ • Dispatch commandes│      │ • slave_state_t       │      │
│  └─────────┬───────────┘      └───────────────────────┘      │
│            │                                                  │
│  ┌─────────▼──────────────────────────────────────────────┐  │
│  │              PERIPHERAL MANAGERS                        │  │
│  │  LED • Heater • Fan • Pump • Hood • MPPT • Switch     │  │
│  └─────────────────────────────────────────────────────────┘  │
│            │                            │                     │
│  ┌─────────▼──────────┐      ┌─────────▼─────────┐          │
│  │  UART Multiplexer  │      │  Ethernet Manager  │          │
│  │  • MPPT x2         │      │  • Slave PCB       │          │
│  │  • Sensors         │      │  • UDP Protocol    │          │
│  └────────────────────┘      └────────────────────┘          │
└─────────────────────────────────────────────────────────────┘
                      │ Ethernet (UDP)
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                    SLAVE PCB (ESP32)                         │
│  • Gestion de l'eau (5 tanks)                               │
│  • Hotte (Hood)                                             │
│  • Capteurs additionnels                                    │
└─────────────────────────────────────────────────────────────┘
```

---

## Architecture de Communication

### Flux de Données

#### 1. **App → MainPCB (Commandes)**
```
App Mobile → BLE → ble_manager → comm_protocol → global_coordinator → Peripheral Manager
```

#### 2. **MainPCB → App (États)**
```
Peripheral Managers → van_state_t → comm_protocol → format_state_json() → ble_manager → App
```

#### 3. **Slave PCB ↔ MainPCB**
```
Slave PCB ⇄ UDP (Ethernet) ⇄ communication_manager ⇄ slave_state_t
```

### Interfaces de Communication

| Interface | Protocole | Fréquence | Direction | Données |
|-----------|-----------|-----------|-----------|---------|
| **BLE** | JSON | Sur demande + 5s | Bidirectionnelle | Commandes & État global |
| **Ethernet** | UDP + Struct binaire | 2s | Bidirectionnelle | État Slave + Commandes |
| **UART1** | VE.Direct | 2s | RX only | MPPT data |
| **UART2** | Custom | Variable | RX only | Sensors (Heater, CO2) |

---

## Gestion d'État Centralisée

### Structure d'État Global (`van_state_t`)

```c
typedef struct {
    // ═══════════════════════════════════════════════════════════
    // ÉNERGIE - MPPT Solar Charge Controllers
    // ═══════════════════════════════════════════════════════════
    struct {
        // MPPT 100|50 (Panneau principal)
        float solar_power_100_50;          // Puissance panneau (W)
        float battery_voltage_100_50;      // Tension batterie (V)
        float battery_current_100_50;      // Courant batterie (A)
        int8_t temperature_100_50;         // Température (°C)
        uint8_t state_100_50;              // État chargeur (0-6)
        uint16_t error_flags_100_50;       // Flags d'erreur
        
        // MPPT 70|15 (Panneau secondaire)
        float solar_power_70_15;
        float battery_voltage_70_15;
        float battery_current_70_15;
        int8_t temperature_70_15;
        uint8_t state_70_15;
        uint16_t error_flags_70_15;
    } mppt;
    
    // ═══════════════════════════════════════════════════════════
    // CAPTEURS - Monitoring environnemental
    // ═══════════════════════════════════════════════════════════
    struct {
        float cabin_temperature;       // Température intérieure (°C)
        float exterior_temperature;    // Température extérieure (°C)
        float humidity;                // Humidité relative (%)
        uint16_t co2_level;            // Niveau CO2 (ppm)
        float water_level;             // Niveau eau (L ou %)
        float battery_voltage;         // Tension batterie mesurée (V)
        bool door_open;                // Porte ouverte/fermée
    } sensors;
    
    // ═══════════════════════════════════════════════════════════
    // CHAUFFAGE - Diesel water heater + Air heater
    // ═══════════════════════════════════════════════════════════
    struct {
        bool heater_on;                // Chauffage diesel ON/OFF
        float target_temperature;      // Consigne température (°C)
        float water_temperature;       // Température eau circuit (°C)
        uint8_t fuel_level_percent;    // Niveau carburant (%)
        uint16_t error_code;           // Code erreur chauffage
        bool pump_active;              // Pompe circulation active
        uint8_t radiator_fan_speed;    // Vitesse ventilateur (0-100%)
    } heater;
    
    // ═══════════════════════════════════════════════════════════
    // ÉCLAIRAGE - LED strips (Interior + Exterior)
    // ═══════════════════════════════════════════════════════════
    struct {
        // LEDs intérieures (plafond - 2 strips)
        struct {
            bool enabled;              // Activé/Désactivé
            uint8_t current_mode;      // Mode: 0=OFF, 1=WHITE, 2=ORANGE, etc.
            uint8_t brightness;        // Luminosité (0-255)
        } roof;
        
        // LEDs placard
        struct {
            bool enabled;
            uint8_t current_mode;
            uint8_t brightness;
        } cabinet;
        
        // LEDs extérieures (avant + arrière)
        struct {
            bool enabled;
            uint8_t current_mode;
            uint8_t brightness;
        } exterior;
    } leds;
    
    // ═══════════════════════════════════════════════════════════
    // SYSTÈME - Status général & erreurs
    // ═══════════════════════════════════════════════════════════
    struct {
        uint32_t uptime;               // Temps de fonctionnement (s)
        bool system_error;             // Erreur système présente
        uint32_t error_code;           // Code erreur global
        system_error_state_t errors;   // État détaillé des erreurs
    } system;
    
} van_state_t;
```

### Structure d'État Slave PCB (`slave_pcb_state_t`)

```c
typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    
    // Gestion de l'eau
    struct {
        water_tank_data_t tank_a;  // Tank A: Eau propre 1
        water_tank_data_t tank_b;  // Tank B: Eau propre 2
        water_tank_data_t tank_c;  // Tank C: Eaux grises
        water_tank_data_t tank_d;  // Tank D: Eaux noires
        water_tank_data_t tank_e;  // Tank E: Récupération pluie
    } water_tanks;
    
    // État de la hotte
    struct {
        bool fan_on;
        uint8_t fan_speed;
    } hood_state;
    
    // Cas d'utilisation actuel
    system_case_t current_case;
    
    // État des erreurs Slave
    slave_error_state_t error_state;
    
    // Santé système
    system_health_t system_health;
    
} slave_pcb_state_t;
```

### Structure Batterie (`battery_state_t`)

```c
typedef struct {
    float voltage;       // Tension batterie (V)
    float current;       // Courant batterie (A) - négatif si charge
    float soc;           // State of Charge (%)
    float temperature;   // Température (°C)
    bool charging;       // En charge ou non
} battery_state_t;
```

---

## Protocole de Communication

### Format des Commandes (App → MainPCB)

#### Structure JSON Générale
```json
{
  "type": "<command_category>",
  "command": "<specific_command>",
  "target": "<optional_target>",
  "value": <numeric_value>,
  "params": {
    // Paramètres additionnels optionnels
  }
}
```

#### Exemples de Commandes

**1. Contrôle LED**
```json
{
  "type": "led",
  "command": "set_mode",
  "target": "roof",
  "value": 2
}
```
Modes LED: `0=OFF, 1=WHITE, 2=ORANGE, 3=FAN, 4=FILM, 5=RAINBOW, 6=DOOR_OPEN`

**2. Contrôle Chauffage**
```json
{
  "type": "heater",
  "command": "set_state",
  "value": 1
}
```

**3. Contrôle Hotte (via Slave)**
```json
{
  "type": "hood",
  "command": "set_state",
  "value": 1
}
```

**4. Gestion Eau (via Slave)**
```json
{
  "type": "water_case",
  "command": "set_case",
  "value": 3
}
```
Cases: `0=RESET, 1-4=SINK, 5-8=SHOWER, 9-10=DRAIN, 11=RAIN`

### Format d'État (MainPCB → App)

#### Structure JSON Complète
```json
{
  "timestamp": 123456789,
  
  "mppt": {
    "solar_power_100_50": 150.5,
    "battery_voltage_100_50": 13.2,
    "battery_current_100_50": 10.5,
    "mppt_state_100_50": 3,
    "solar_power_70_15": 80.3,
    "battery_voltage_70_15": 13.3,
    "total_solar_power": 230.8
  },
  
  "sensors": {
    "cabin_temperature": 22.5,
    "exterior_temperature": 15.3,
    "humidity": 45.0,
    "co2_level": 450,
    "water_level": 85.0,
    "battery_voltage": 13.2,
    "door_open": false
  },
  
  "heater": {
    "heater_on": true,
    "target_temperature": 20.0,
    "water_temperature": 45.0,
    "fuel_level_percent": 75,
    "error_code": 0
  },
  
  "leds": {
    "roof": {
      "enabled": true,
      "current_mode": 1,
      "brightness": 200
    },
    "cabinet": {
      "enabled": false,
      "current_mode": 0,
      "brightness": 0
    },
    "exterior": {
      "enabled": false,
      "current_mode": 0,
      "brightness": 0
    }
  },
  
  "battery": {
    "voltage": 13.2,
    "current": -5.5,
    "soc": 85.0,
    "charging": true,
    "temperature": 25.0
  },
  
  "slave_pcb": {
    "connected": true,
    "hood": {
      "fan_on": false,
      "fan_speed": 0
    },
    "water_case": {
      "current_case": 0,
      "valve_open": false
    }
  },
  
  "system": {
    "uptime": 3600,
    "system_error": false,
    "error_code": 0
  }
}
```

**Note:** L'état complet est envoyé toutes les 5 secondes via BLE automatiquement.

---

## Global Coordinator

### Principe de Fonctionnement

Le Global Coordinator agit comme un **Event Bus** centralisé utilisant le pattern **Publisher-Subscriber**.

#### Architecture

```c
┌─────────────────────────────────────────────────────────┐
│              GLOBAL COORDINATOR CORE                     │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │         Event Queue (FreeRTOS Queue)               │ │
│  │  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐           │ │
│  │  │ Evt1 │→ │ Evt2 │→ │ Evt3 │→ │ Evt4 │→ ...      │ │
│  │  └──────┘  └──────┘  └──────┘  └──────┘           │ │
│  └────────────────────────────────────────────────────┘ │
│                          │                              │
│                          ▼                              │
│  ┌────────────────────────────────────────────────────┐ │
│  │         Subscriber Registry (8 max)                │ │
│  │  Event Type → Callback Functions                   │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Types d'Événements

```c
typedef enum {
    // ═══════════════ ÉVÉNEMENTS SWITCH ═══════════════
    GC_EVT_SWITCH_SHORT_CLICK,      // value = nombre de clics (1-6)
    GC_EVT_SWITCH_LONG_PRESS_VALUE, // value = luminosité (10-255)
    GC_EVT_SWITCH_LONG_PRESS_FINAL, // value = luminosité finale
    
    // ═══════════════ ÉVÉNEMENTS PORTE ═══════════════
    GC_EVT_DOOR_VALUE_CHANGED,      // value = 1 (ouvert) / 0 (fermé)
    
    // ═══════════════ ÉVÉNEMENTS LED ═══════════════
    GC_EVT_LED_MODE_CHANGE,         // value = nouveau mode
    GC_EVT_LED_BRIGHTNESS_CHANGE,   // value = nouvelle luminosité
    
    // ═══════════════ ÉVÉNEMENTS CHAUFFAGE ═══════════════
    GC_EVT_HEATER_STATE_CHANGE,     // value = 0/1 (OFF/ON)
    GC_EVT_HEATER_TEMP_CHANGE,      // value = température cible
    
    // ═══════════════ ÉVÉNEMENTS HOTTE ═══════════════
    GC_EVT_HOOD_STATE_CHANGE,       // value = 0/1 (OFF/ON)
    
    // ═══════════════ ÉVÉNEMENTS SYSTÈME ═══════════════
    GC_EVT_ERROR_OCCURRED,          // value = error code
    GC_EVT_STATE_UPDATE_REQUEST,    // value = 0
    
} gc_event_type_t;
```

### API du Global Coordinator

```c
// ══════════════════════════════════════════════════════════
// INITIALISATION
// ══════════════════════════════════════════════════════════
esp_err_t global_coordinator_init(void);

// ══════════════════════════════════════════════════════════
// PUBLICATION D'ÉVÉNEMENTS
// ══════════════════════════════════════════════════════════
esp_err_t global_coordinator_publish(gc_event_type_t type, int value);
// Exemple: global_coordinator_publish(GC_EVT_LED_MODE_CHANGE, 2);

// ══════════════════════════════════════════════════════════
// SOUSCRIPTION À DES ÉVÉNEMENTS
// ══════════════════════════════════════════════════════════
typedef void (*gc_event_callback_t)(gc_event_t evt);
esp_err_t global_coordinator_subscribe(gc_event_type_t type, 
                                       gc_event_callback_t callback);

// Exemple d'utilisation:
void my_led_callback(gc_event_t evt) {
    if (evt.type == GC_EVT_LED_MODE_CHANGE) {
        led_set_mode(LED_ROOF_STRIP_1, (led_mode_type_t)evt.value);
    }
}

// Dans init():
global_coordinator_subscribe(GC_EVT_LED_MODE_CHANGE, my_led_callback);
```

### Flux de Traitement des Commandes

```
┌──────────────────────────────────────────────────────────────┐
│ 1. App envoie commande JSON via BLE                          │
└────────────────────┬─────────────────────────────────────────┘
                     ▼
┌──────────────────────────────────────────────────────────────┐
│ 2. ble_manager reçoit et transmet à comm_protocol           │
└────────────────────┬─────────────────────────────────────────┘
                     ▼
┌──────────────────────────────────────────────────────────────┐
│ 3. comm_protocol parse JSON et identifie type/commande      │
└────────────────────┬─────────────────────────────────────────┘
                     ▼
┌──────────────────────────────────────────────────────────────┐
│ 4. comm_protocol publie événement via global_coordinator    │
│    global_coordinator_publish(GC_EVT_LED_MODE_CHANGE, 2);   │
└────────────────────┬─────────────────────────────────────────┘
                     ▼
┌──────────────────────────────────────────────────────────────┐
│ 5. Les modules souscripteurs reçoivent le callback          │
│    • led_coordinator → led_manager → Actions physiques      │
└────────────────────┬─────────────────────────────────────────┘
                     ▼
┌──────────────────────────────────────────────────────────────┐
│ 6. Mise à jour de van_state_t                                │
└────────────────────┬─────────────────────────────────────────┘
                     ▼
┌──────────────────────────────────────────────────────────────┐
│ 7. Broadcast automatique vers App (toutes les 5s)           │
└──────────────────────────────────────────────────────────────┘
```

---

# Peripheral Devices Module Documentation

## Overview
The `peripherals_devices` folder contains all hardware device managers for the van's electrical systems. Each manager handles initialization, control, and monitoring of specific peripherals.

---

## fan_manager.c/h

**Description:**  
Controls the PWM PC Fan of the radiator using LEDC (LED Control) peripheral. Operates at 25kHz with 8-bit resolution (0-255 duty cycle).

**API:**

```c
esp_err_t fan_manager_init(void);
```
Initializes LEDC timer and channel for PWM fan control on `FAN_HEATER_PWM` pin. Starts with fan off (0% duty).

```c
esp_err_t fan_manager_set_speed(uint8_t speed_percent);
```
Sets fan speed as percentage (0-100%). Internally converts to 8-bit PWM duty cycle. Stores current speed.

```c
uint8_t fan_manager_get_speed(void);
```
Returns the currently set fan speed percentage (not measured from sensor).

---

## heater_manager.c/h

**Description:**  
Manages both air heater (radiator with fan + circulation pump) and diesel water heater systems. Monitors fuel level via analog gauge and controls heater power signals.

**API:**

```c
esp_err_t heater_manager_init(void);
```
Initializes GPIO pins for diesel heater control, fuel gauge ADC (ADC2), and sub-modules (pump_manager, fan_manager).

```c
uint8_t heater_manager_get_fuel_level(void);
```
Reads fuel level from resistive gauge (0-190Ω) via ADC. Returns fuel level as percentage (0-100%).

```c
esp_err_t heater_manager_set_air_heater(bool state, uint8_t fan_speed_percent);
```
Controls the air heating system by setting radiator fan speed and circulation pump state.

```c
esp_err_t heater_manager_set_diesel_water_heater(bool state, uint8_t temperature);
```
Controls diesel heater power signal. Temperature control via UART protocol is TODO.

---

## hood_manager.c/h

**Description:**  
Simple on/off control for kitchen hood extraction fan.

**API:**

```c
esp_err_t hood_init(void);
```
Configures `HOOD_FAN` GPIO as output and ensures fan starts in OFF state.

```c
void hood_set_state(hood_state_t state);
```
Turns hood fan ON or OFF. State enum: `HOOD_OFF` (0), `HOOD_ON` (1).

---

## led_manager.c/h

**Description:**  
Main LED control module managing 4 LED strips (2 interior roof, 2 exterior). Supports static modes (white, orange, film) and dynamic animations (rainbow, door open/close). Uses RMT peripheral with SK6812 RGBW LEDs.

**API:**

```c
esp_err_t led_manager_init(void);
```
Initializes LED strips via RMT channels, configures exterior LED power GPIO, and starts LED manager task.

```c
esp_err_t led_set_mode(led_strip_t strip, led_mode_type_t mode);
```
Sets LED mode for specified strip. Modes: `LED_MODE_OFF`, `LED_MODE_WHITE`, `LED_MODE_ORANGE`, `LED_MODE_FAN`, `LED_MODE_FILM`, `LED_MODE_RAINBOW`, `LED_MODE_DOOR_OPEN`, `LED_MODE_DOOR_TIMEOUT`.

```c
esp_err_t led_set_brightness(led_strip_t strip, uint8_t brightness);
```
Sets brightness (0-255) and reapplies current mode to specified strip.

```c
uint8_t led_get_brightness(led_strip_t strip);
```
Returns current brightness level of the strip.

```c
bool led_is_strip_on(led_strip_t strip);
```
Checks if strip is in any mode other than OFF.

```c
esp_err_t led_set_exterior_power(bool enabled);
```
Controls power to exterior LEDs via `EXT_LED` GPIO.

```c
bool led_is_door_animation_active(void);
void led_set_door_animation_active(bool active);
```
Get/set door animation active flag.

```c
esp_err_t led_trigger_door_animation(void);
esp_err_t led_trigger_error_mode(void);
```
Trigger special animation modes.

```c
led_strip_handle_t led_manager_get_handle(led_strip_t strip);
int led_manager_get_led_count(led_strip_t strip);
```
Helper functions to retrieve strip handle and LED count for a given strip.

---

## led_coordinator.c/h

**Description:**  
Event-driven coordinator that subscribes to global events (switch clicks, door state) and translates them into LED control actions. Maps multi-click patterns to LED modes and manages brightness during long-press.

**API:**

```c
esp_err_t led_coordinator_init(void);
```
Subscribes to global coordinator events: `GC_EVT_SWITCH_SHORT_CLICK`, `GC_EVT_SWITCH_LONG_PRESS_VALUE`, `GC_EVT_SWITCH_LONG_PRESS_FINAL`, `GC_EVT_DOOR_VALUE_CHANGED`.

**Click Mapping:**
- 1 click: OFF
- 2 clicks: White
- 3 clicks: Orange
- 4 clicks: Interior + Exterior white
- 5 clicks: Film mode
- 6 clicks: Rainbow animation

---

## led_static_modes.c/h

**Description:**  
Static LED color implementations. Handles RMT initialization and solid color fills.

**API:**

```c
esp_err_t led_static_init_strips(led_strip_handle_t strips[]);
```
Initializes all LED strips with RMT configuration (10MHz, SK6812 RGBW format).

```c
void led_static_off(led_strip_t strip, uint8_t brightness);
```
Turns off all LEDs (sets RGBW to 0,0,0,0).

```c
void led_static_white(led_strip_t strip, uint8_t brightness);
```
Sets all LEDs to white using W channel (0,0,0,255) scaled by brightness.

```c
void led_static_orange(led_strip_t strip, uint8_t brightness);
```
Sets all LEDs to orange (220,120,0,0) scaled by brightness.

```c
void led_static_film(led_strip_t strip, uint8_t brightness);
```
Sets all LEDs to dim warm light (30,10,0,0) for film watching.

---

## led_dynamic_modes.c/h

**Description:**  
Animated LED effects running as FreeRTOS tasks. Includes rainbow color wheel and door open/close sunrise/sunset animations.

**API:**

```c
esp_err_t led_dynamic_rainbow(led_strip_t strip, uint8_t brightness);
```
Starts rainbow animation task (~20 FPS color wheel cycle).

```c
esp_err_t led_dynamic_door_open(led_strip_t strip, uint8_t brightness, bool direction);
```
Starts door animation: `direction=true` for intro (sunrise wave, 5s), `direction=false` for outro (sunset fade, 60s).

```c
void led_dynamic_stop(led_strip_t strip);
```
Requests animation task to stop by setting stop flag.

---

## mppt_manager.c/h

**Description:**  
Manages communication with two Victron MPPT solar charge controllers (100/50 and 70/15) via VE.Direct protocol over UART multiplexer. Parses data frames and monitors battery voltage, current, power, temperature, and charger state.

**API:**

```c
esp_err_t mppt_manager_init(void);
```
Initializes MPPT data structures and creates task on CPU1 for load balancing.

```c
void mppt_manager_task(void *parameters);
```
Task that reads VE.Direct frames from both MPPTs via UART mux every 2 seconds. Parses fields: V (voltage), I (current), PPV (panel power), CS (charger state), T (temperature). Monitors for communication timeouts (>30s).

**Note:** Currently commented out error reporting and communication message sending.

---

## pump_manager.c/h

**Description:**  
Simple GPIO control for antifreeze circulation pump used with the air heater system.

**API:**

```c
esp_err_t pump_manager_init(void);
```
Configures `PH` GPIO as output and initializes pump to OFF state.

```c
esp_err_t pump_manager_set_state(bool enabled);
```
Turns circulation pump ON (true) or OFF (false).

```c
bool pump_manager_get_state(void);
```
Returns current pump state by reading GPIO level.

---

## switch_manager.c/h

**Description:**  
Input handler for physical switch and door sensor. Debounces inputs, detects short/long press patterns, counts multi-clicks, and generates progressive brightness values (10-255) during long press. Publishes events to global coordinator.

**API:**

```c
esp_err_t switch_manager_init(void);
```
Configures GPIO inputs for switch (`INTER`) and door sensor (`VAN_LIGHT`). Creates switch manager task at 20Hz update rate.

```c
void switch_manager_task(void *parameters);
```
Main task that polls switch/door inputs and handles state machine for click detection and long press.

**Configuration Constants:**
- `SWITCH_DEBOUNCE_MS`: 50ms
- `SWITCH_SHORT_PRESS_MS`: 500ms (max for short click)
- `SWITCH_MULTI_CLICK_MS`: 700ms (window for multi-click detection)
- `SWITCH_LONG_PRESS_MS`: 1000ms (threshold for long press)
- `SWITCH_LONG_CYCLE_MS`: 5000ms (cycle time for brightness ramp 10→255→10)

**Events Published:**
- `GC_EVT_SWITCH_SHORT_CLICK`: with click count (1-6)
- `GC_EVT_SWITCH_LONG_PRESS_VALUE`: with brightness value (10-255)
- `GC_EVT_SWITCH_LONG_PRESS_FINAL`: with final brightness value
- `GC_EVT_DOOR_VALUE_CHANGED`: 1=door open, 0=door closed/timeout

**Note:** Reads combined input from UART simulation (priority) and physical GPIO button.

---

## Gestion des Erreurs

### Architecture du Error Manager

Le système d'erreur est centralisé et hiérarchisé avec 4 niveaux de sévérité.

#### Structure d'Erreur

```c
typedef struct {
    main_pcb_err_t error_code;      // Code erreur (0x1XXX - 0x5XXX)
    error_severity_t severity;       // INFO, WARNING, ERROR, CRITICAL
    error_category_t category;       // INIT, COMM, DEVICE, SENSOR, etc.
    uint32_t timestamp;              // Timestamp (ms)
    char module[32];                 // Module source
    char description[64];            // Description
    uint32_t data;                   // Données additionnelles
} error_event_t;
```

#### Niveaux de Sévérité

| Niveau | Description | Action |
|--------|-------------|--------|
| **INFO** | Événement informationnel | Log seulement |
| **WARNING** | Avertissement, surveillance requise | Log + monitoring |
| **ERROR** | Erreur, fonctionnalité impactée | Log + notification + recovery |
| **CRITICAL** | Critique, sécurité compromise | Log + notification + protocole sécurité |

#### Catégories d'Erreurs

```c
typedef enum {
    ERR_CAT_INIT = (1 << 0),      // Erreurs d'initialisation
    ERR_CAT_COMM = (1 << 1),      // Erreurs de communication
    ERR_CAT_DEVICE = (1 << 2),    // Erreurs de périphériques
    ERR_CAT_SENSOR = (1 << 3),    // Erreurs de capteurs
    ERR_CAT_ACTUATOR = (1 << 4),  // Erreurs d'actionneurs
    ERR_CAT_SYSTEM = (1 << 5),    // Erreurs système
    ERR_CAT_CASE = (1 << 6),      // Erreurs de cas d'usage
    ERR_CAT_SAFETY = (1 << 7)     // Erreurs de sécurité
} error_category_t;
```

#### Codes d'Erreurs Principaux

| Range | Catégorie | Exemples |
|-------|-----------|----------|
| 0x1XXX | Initialization | INVALID_ARG, INIT_FAIL, MEMORY |
| 0x2XXX | Communication | COMM_FAIL, I2C_FAIL, TIMEOUT, ETH_DISCONNECTED |
| 0x3XXX | Device | DEVICE_NOT_FOUND, DEVICE_BUSY, DEVICE_FAULT |
| 0x4XXX | State/Case | STATE_INVALID, INCOMPATIBLE_CASE |
| 0x5XXX | Safety | SAFETY_LIMIT, EMERGENCY_STOP, OVERCURRENT |

#### Utilisation

```c
// Macro simplifiée pour reporter une erreur
REPORT_ERROR(MAIN_PCB_ERR_COMM_FAIL, TAG, "Failed to connect", errno);

// Accès aux statistiques
error_stats_t stats;
error_get_stats(&stats);
ESP_LOGI(TAG, "Total errors: %lu", stats.total_errors);

// Vérifier criticité
if (error_is_critical(error_code)) {
    // Actions d'urgence
}

// Obtenir l'état complet pour transmission
system_error_state_t* error_state = error_get_system_state();
```

### Propagation des Erreurs

```
┌─────────────────────────────────────────────────────────┐
│ Périphérique détecte erreur                             │
│ (LED strip, MPPT, Heater, etc.)                         │
└──────────────────┬──────────────────────────────────────┘
                   │ REPORT_ERROR()
                   ▼
┌─────────────────────────────────────────────────────────┐
│ error_manager.c                                          │
│ • Log avec couleurs selon sévérité                      │
│ • Mise à jour statistiques                              │
│ • Stockage dans historique (5 dernières erreurs)        │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│ system_error_state_t                                     │
│ • Intégré dans van_state_t                              │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│ comm_protocol broadcast vers App                        │
│ • App affiche alertes utilisateur                       │
└─────────────────────────────────────────────────────────┘
```

---

## Ajout de Nouveaux Composants

### Checklist Complète

#### 1. **Créer le Manager du Périphérique**

**Fichiers:** `new_device_manager.c` et `new_device_manager.h`

```c
// new_device_manager.h
#pragma once
#include "esp_err.h"
#include "../common_includes/gpio_pinout.h"

// Initialisation
esp_err_t new_device_init(void);

// Contrôle
esp_err_t new_device_set_state(bool state);
esp_err_t new_device_set_value(uint8_t value);

// Lecture
uint8_t new_device_get_state(void);
```

```c
// new_device_manager.c
#include "new_device_manager.h"
#include "esp_log.h"

static const char *TAG = "NEW_DEVICE";

esp_err_t new_device_init(void) {
    ESP_LOGI(TAG, "Initializing new device...");
    
    // Configuration GPIO/périphérique
    // ...
    
    ESP_LOGI(TAG, "New device initialized");
    return ESP_OK;
}

esp_err_t new_device_set_state(bool state) {
    ESP_LOGI(TAG, "Setting state to %s", state ? "ON" : "OFF");
    // Implémentation
    return ESP_OK;
}
```

#### 2. **Ajouter au CMakeLists.txt**

```cmake
# peripherals_devices/CMakeLists.txt
idf_component_register(
    SRCS 
        "fan_manager.c"
        "heater_manager.c"
        # ... autres managers existants
        "new_device_manager.c"    # ← AJOUTER ICI
    INCLUDE_DIRS "."
)
```

#### 3. **Étendre van_state_t**

```c
// communication_protocol.h
typedef struct {
    // ... structures existantes (mppt, sensors, heater, leds, system)
    
    // ═══════════════════════════════════════════════════════════
    // NOUVEAU COMPOSANT
    // ═══════════════════════════════════════════════════════════
    struct {
        bool enabled;
        uint8_t current_state;
        uint8_t value;
        uint16_t error_code;
    } new_device;
    
} van_state_t;
```

#### 4. **Ajouter des Événements au Global Coordinator**

```c
// global_coordinator.h
typedef enum {
    // ... événements existants
    
    // Nouveaux événements pour le composant
    GC_EVT_NEW_DEVICE_STATE_CHANGE,
    GC_EVT_NEW_DEVICE_VALUE_CHANGE,
    
} gc_event_type_t;
```

#### 5. **Créer un Coordinator (si nécessaire)**

```c
// new_device_coordinator.c
#include "new_device_manager.h"
#include "../main/global_coordinator.h"

static void new_device_state_callback(gc_event_t evt) {
    ESP_LOGI(TAG, "Received state change event: %d", evt.value);
    new_device_set_state(evt.value);
}

esp_err_t new_device_coordinator_init(void) {
    // S'abonner aux événements pertinents
    global_coordinator_subscribe(GC_EVT_NEW_DEVICE_STATE_CHANGE, 
                                 new_device_state_callback);
    return ESP_OK;
}
```

#### 6. **Ajouter au Protocole de Communication**

```c
// communication_protocol.h
typedef enum {
    // ... commandes existantes
    CMD_SET_NEW_DEVICE_STATE,
    CMD_SET_NEW_DEVICE_VALUE,
} new_device_command_t;

// communication_protocol.c
static esp_err_t process_new_device_command(const cJSON* cmd_json) {
    cJSON *cmd = cJSON_GetObjectItem(cmd_json, "command");
    cJSON *value = cJSON_GetObjectItem(cmd_json, "value");
    
    if (strcmp(cmd->valuestring, "set_state") == 0) {
        // Publier événement
        global_coordinator_publish(GC_EVT_NEW_DEVICE_STATE_CHANGE, 
                                  value->valueint);
    }
    
    return ESP_OK;
}

// Dans comm_protocol_process_command():
else if (strcmp(type_str, "new_device") == 0) {
    ret = process_new_device_command(root);
}
```

#### 7. **Ajouter au format_state_json()**

```c
// communication_protocol.c - dans format_state_json()

// New device data
cJSON *new_device = cJSON_CreateObject();
cJSON_AddBoolToObject(new_device, "enabled", van_state.new_device.enabled);
cJSON_AddNumberToObject(new_device, "current_state", van_state.new_device.current_state);
cJSON_AddNumberToObject(new_device, "value", van_state.new_device.value);
cJSON_AddNumberToObject(new_device, "error_code", van_state.new_device.error_code);
cJSON_AddItemToObject(root, "new_device", new_device);
```

#### 8. **Initialiser dans main.c**

```c
// main.c - dans app_main()

ESP_LOGI(TAG, "Initializing new device...");
ESP_ERROR_CHECK(new_device_init());

ESP_LOGI(TAG, "Initializing new device coordinator...");
ESP_ERROR_CHECK(new_device_coordinator_init());
```

#### 9. **Mettre à jour Global_idea.md**

Ajouter la documentation complète du nouveau module dans la section "Peripheral Devices Module Documentation".

#### 10. **Tester**

1. **Test unitaire du manager:**
   ```c
   new_device_init();
   new_device_set_state(true);
   assert(new_device_get_state() == true);
   ```

2. **Test via événements:**
   ```c
   global_coordinator_publish(GC_EVT_NEW_DEVICE_STATE_CHANGE, 1);
   vTaskDelay(pdMS_TO_TICKS(100));
   // Vérifier que l'état a changé
   ```

3. **Test via App:**
   - Envoyer commande JSON depuis l'app
   - Vérifier que l'état change
   - Vérifier que le broadcast contient les nouvelles données

---

## Exemple Complet: Ajout d'un Ventilateur de Plafond

### 1. Créer ceiling_fan_manager.c/h

```c
// ceiling_fan_manager.h
#pragma once
#include "esp_err.h"

typedef enum {
    FAN_SPEED_OFF = 0,
    FAN_SPEED_LOW,
    FAN_SPEED_MEDIUM,
    FAN_SPEED_HIGH
} ceiling_fan_speed_t;

esp_err_t ceiling_fan_init(void);
esp_err_t ceiling_fan_set_speed(ceiling_fan_speed_t speed);
ceiling_fan_speed_t ceiling_fan_get_speed(void);
```

### 2. Étendre van_state_t

```c
struct {
    bool enabled;
    uint8_t speed;  // 0=OFF, 1=LOW, 2=MED, 3=HIGH
} ceiling_fan;
```

### 3. Ajouter événement

```c
GC_EVT_CEILING_FAN_SPEED_CHANGE,
```

### 4. Ajouter commande JSON

```json
{
  "type": "ceiling_fan",
  "command": "set_speed",
  "value": 2
}
```

### 5. Résultat dans l'état JSON

```json
"ceiling_fan": {
  "enabled": true,
  "speed": 2
}
```

---

## Recommandations Finales

### Architecture Modulaire

✅ **Faire:**
- Un manager par périphérique physique
- Un coordinator par logique métier complexe
- Utiliser le global_coordinator pour tout événement
- Centraliser l'état dans van_state_t
- Documenter chaque ajout dans Global_idea.md

❌ **Éviter:**
- Accès directs entre managers
- Duplication d'état
- Communication point-à-point
- Code spécifique à l'app dans les managers

### Performance

- Les événements sont traités de manière asynchrone (queue)
- Le broadcast BLE est limité à 5 secondes
- Les états sont mis à jour atomiquement
- Les mutex protègent les accès concurrents

### Maintenance

1. **Ajouter un test pour chaque nouveau composant**
2. **Versionner le protocole JSON si modifications majeures**
3. **Logger tous les événements importants**
4. **Utiliser le error_manager pour toutes les erreurs**
5. **Maintenir cette documentation à jour**

---

## Diagramme de Séquence Complet

```
App Mobile          BLE Manager      Comm Protocol    Global Coord    LED Manager
    │                    │                │                │               │
    │─── {"type":"led"} ─→│                │                │               │
    │                    │                │                │               │
    │                    │─── JSON ──────→│                │               │
    │                    │                │                │               │
    │                    │                │── publish() ──→│               │
    │                    │                │                │               │
    │                    │                │                │─── callback ─→│
    │                    │                │                │               │
    │                    │                │                │               │─ GPIO
    │                    │                │                │               │
    │                    │                │ Update         │               │
    │                    │                │← van_state_t ──│               │
    │                    │                │                │               │
    │                    │ format_state() │                │               │
    │                    │← JSON ─────────│                │               │
    │                    │                │                │               │
    │←─── JSON state ────│                │                │               │
    │                    │                │                │               │
```

---

**Documentation complète. Système prêt pour extension et maintenance facile! 🚐⚡**