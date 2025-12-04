# Système de Vidéoprojecteur Motorisé - ESP32-C3 Xiao

## Description

Système de commande pour un vidéoprojecteur motorisé monté/descendant via un moteur DC avec encodeur rotatif. Compatible avec les commandes bouton local et BLE.

### Fonctionnalités

- ✅ **Contrôle moteur DC** : Montée/descente du vidéoprojecteur via DRV8833
- ✅ **Encodeur rotatif** : Retour de position via quadrature
- ✅ **Bouton local** : Appui court = toggle, appui long = fonction future
- ✅ **LED indicatrice** : État du vidéoprojecteur
- ✅ **Protection USB** : Détection d'alimentation vidéoprojecteur pour éviter rétraction
- ✅ **Commande IR** : LED IR pour télécommande (protocole à déterminer)
- ✅ **BLE NimBLE** : Commandes distantes et notification d'état

## Brochage - ESP32-C3 Xiao

| Fonction | GPIO | Broche |
|----------|------|--------|
| Moteur SLEEP | D5 | 5 |
| Moteur DIR1 | D6 | 6 |
| Moteur DIR2 | D7 | 7 |
| Encodeur A | D2 | 2 |
| Encodeur B | D3 | 3 |
| Bouton | D9 | 9 |
| LED Bouton | D10 | 10 |
| Drapeau USB | D1 | 1 |
| LED IR | D8 | 8 |


### Configuration

Dans `menuconfig`, :

- **Moteur DC**
  - Tours d'encodeur pour course complète
  - Ratio de réduction

- **BLE**
  - Nom du device
  - Intervalle d'advertising

- **Bouton**
  - Temps debounce
  - Durée pour appui long

- **LED IR**
  - Fréquence porteuse
  - Rapport cyclique

## API

### Motor Manager

```c
// Initialisation
int motor_manager_init(const motor_config_t *config);

// Déploiement/Rétraction
int motor_manager_deploy_video_proj(void);
int motor_manager_retract_video_proj(void);

// Contrôle bas niveau
int motor_manager_turn(float n_turns, motor_direction_t direction);
void motor_manager_stop(void);

// Récupération d'état
bool motor_manager_is_deployed(void);
int32_t motor_manager_get_position(void);
```

### Button Manager

```c
int button_manager_init(uint8_t pin_button, uint8_t pin_led, 
                        button_callback_t callback);
void button_manager_set_led(bool enabled);
bool button_manager_get_led_state(void);
```

### USB Manager

```c
int usb_manager_init(uint8_t pin_usb_flag, usb_power_callback_t callback);
bool usb_manager_is_powered(void);
bool usb_manager_get_flag(void);
```

### IR LED Manager

```c
int ir_led_manager_init(uint8_t pin_ir, const ir_config_t *config);
int ir_led_manager_send_command(const uint8_t *data, uint16_t length);
void ir_led_manager_enable(void);
void ir_led_manager_disable(void);
bool ir_led_manager_is_enabled(void);
```

### BLE Manager

```c
int ble_manager_init(const char *device_name, ble_command_callback_t callback);
int ble_manager_start_advertising(void);
int ble_manager_stop_advertising(void);
int ble_manager_notify_status(bool is_deployed);
bool ble_manager_is_connected(void);
```

## Commandes BLE

| Commande | Code | Description |
|----------|------|-------------|
| `BLE_CMD_DEPLOY` | 0 | Déploie le vidéoprojecteur |
| `BLE_CMD_RETRACT` | 1 | Rétracte le vidéoprojecteur |
| `BLE_CMD_STOP` | 2 | Arrête le moteur |
| `BLE_CMD_GET_STATUS` | 3 | Demande l'état |

### Services GATT

**Service vidéoprojecteur (UUID: 0x181A)**

- **Contrôle (UUID: 0x2A58)** - Write
  - Reçoit les commandes
  
- **Statut (UUID: 0x2A19)** - Read/Notify
  - Retourne l'état (0=rétracté, 1=déployé)




