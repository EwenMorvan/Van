# Système de Simulation - Van Management

## Vue d'ensemble

Ce système inclut une fonction de simulation qui génère des données réalistes pour tous les capteurs et systèmes du van. Cela permet de tester l'interface utilisateur et les communications avant de connecter le vrai matériel.

## Activation/Désactivation

### Pour ACTIVER la simulation :
Dans `main/protocol.h`, ligne ~140 :
```c
#define ENABLE_SIMULATION 1  // Simulation ON
```

### Pour DÉSACTIVER la simulation :
Dans `main/protocol.h`, ligne ~140 :
```c
#define ENABLE_SIMULATION 0  // Simulation OFF
```

## Données simulées

### 🔋 MPPT (Panneaux solaires)
- **Solar Power** : Varie selon un cycle "jour/nuit" de 60 secondes
- **Battery Voltage** : 12.4V - 13.2V (réaliste pour batteries LiFePO4)
- **Battery Current** : Calculé automatiquement (Power/Voltage)
- **Temperature** : 28-45°C selon la "production solaire"
- **State** : Bulk charging quand il y a du "soleil", Off sinon

### 🌡️ Capteurs
- **Fuel Level** : 20% - 80% (cycle lent)
- **Cabin Temperature** : 14°C - 30°C
- **Onboard Temperature** : 15°C - 35°C  
- **Humidity** : 40% - 80%
- **CO2 Level** : 400 - 1200 ppm (cycle rapide)
- **Light Level** : 0 - 1023 (ADC 10-bit)
- **Van Light** : Activé automatiquement quand light_level < 300

### 🔥 Chauffage
- **Water Temperature** : Suit la température cible avec inertie thermique
- **Pump Active** : Activé pendant le chauffage
- **Radiator Fan** : Varie selon l'état du chauffage

### 💨 Ventilateurs
- **Elec Box Fan** : Vitesse selon température interne
- **Heater Fan** : Actif quand chauffage ON
- **Hood Fan** : Actif 5 secondes toutes les 40 secondes

### 💡 LEDs
- **Switch Pressed** : Simulé toutes les 15 secondes
- **Exterior Power** : Suit l'état de la lumière du van
- **Error Mode** : Activé quand fuel < 10%

### ⚙️ Système
- **Uptime** : Réel (depuis le démarrage)
- **Slave PCB** : Déconnecté 5 secondes toutes les 60 secondes
- **Errors** : Erreur fuel automatique quand niveau < 10%

## Cycles de simulation

- **Cycle lent** : 60 secondes (températures, fuel, solaire)
- **Cycle rapide** : 10 secondes (CO2, lumière, ventilateurs)
- **Événements** : Porte, switch LED, erreurs

## Suppression complète

Quand vous voulez supprimer la simulation :

1. **Dans `protocol.h`** : Supprimez les lignes ~140-145 :
```c
// SIMULATION FUNCTIONS - Remove when real hardware is connected
#define ENABLE_SIMULATION 1  // Set to 0 to disable simulation
#if ENABLE_SIMULATION
void protocol_simulate_sensor_data(void);
#endif
```

2. **Dans `protocol.c`** : Supprimez :
   - Les lignes de simulation dans `protocol_get_state()`
   - Toute la fonction `protocol_simulate_sensor_data()` (environ 120 lignes)

3. **Supprimez ce fichier** : `SIMULATION_README.md`

## Test des commandes

Avec la simulation active, vous pouvez tester :
- Changement de température cible → La simulation suit avec inertie
- ON/OFF chauffage → Impact sur ventilateurs et pompe
- Modes LED → Changements visibles dans les données
- Les erreurs sont automatiquement simulées selon le niveau de fuel

## Performance

La simulation ajoute environ 100-200 µs de traitement par appel à `protocol_get_state()`.
Avec un intervalle BLE de 100ms, l'impact CPU est négligeable.
