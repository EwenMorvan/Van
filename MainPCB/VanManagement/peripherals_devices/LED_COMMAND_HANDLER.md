# LED Command Handler

## Description

Ce module gère l'application des commandes LED reçues depuis l'application mobile via BLE. Il interprète les commandes `van_command_t` et applique exactement la configuration demandée sur les bandeaux LED.

## Fonctionnalités

### Modes Statiques
- Applique des couleurs personnalisées à chaque LED individuellement
- Supporte tous les strips : intérieur (ROOF1, ROOF2) et extérieur (FRONT, BACK)
- Contrôle individuel de R, G, B, W et luminosité pour chaque LED

### Modes Dynamiques (Animations personnalisées)
- Système de keyframes avec interpolation temporelle
- 3 modes de transition :
  - `TRANSITION_LINEAR` : interpolation linéaire
  - `TRANSITION_EASE_IN_OUT` : transition douce (ease in/out)
  - `TRANSITION_STEP` : changement instantané
- 3 comportements de boucle :
  - `LOOP_BEHAVIOR_ONCE` : joue une fois puis s'arrête
  - `LOOP_BEHAVIOR_REPEAT` : boucle infinie
  - `LOOP_BEHAVIOR_PING_PONG` : va-et-vient (forward/reverse)

## Architecture

```
led_command_handler.c/h
├── led_apply_command()          [Point d'entrée principal]
│   ├── apply_static_command()   [Gestion des modes statiques]
│   └── apply_dynamic_command()  [Gestion des animations]
│       └── custom_animation_task() [Task FreeRTOS pour l'animation]
```

## Utilisation

### Dans main.c

```c
void handle_van_command(van_command_t* cmd) {
    if (cmd->type == COMMAND_TYPE_LED) {
        esp_err_t ret = led_apply_command(cmd);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to apply LED command");
        }
    }
}
```

## Flux de données

```
App Mobile (BLE)
    ↓
Fragment Handler (réassemblage)
    ↓
Command Parser (parsing)
    ↓
van_command_t
    ↓
led_apply_command() ← [CE MODULE]
    ↓
LED Hardware (bandeaux physiques)
```

## Mapping des strips

### Statique (led_strip_static_target_t → led_strip_t)
- `ROOF_LED1` → `LED_ROOF_STRIP_1`
- `ROOF_LED2` → `LED_ROOF_STRIP_2`
- `ROOF_LED_ALL` → `LED_ROOF_STRIP_1` + `LED_ROOF_STRIP_2`
- `EXT_AV_LED` → `LED_EXT_FRONT`
- `EXT_AR_LED` → `LED_EXT_BACK`
- `EXT_LED_ALL` → `LED_EXT_FRONT` + `LED_EXT_BACK`

### Dynamique (led_strip_dynamic_target_t → led_strip_t)
- `ROOF_LED1_DYNAMIC` → `LED_ROOF_STRIP_1`
- `ROOF_LED2_DYNAMIC` → `LED_ROOF_STRIP_2`
- `ROOF_LED_ALL_DYNAMIC` → `LED_ROOF_STRIP_1` + `LED_ROOF_STRIP_2`

## Détails techniques

### Gestion de la mémoire
- Les animations dynamiques créent des tasks FreeRTOS (1 par strip)
- Stack size : 8192 bytes par task
- Priorité : 6 (même que led_manager)
- CPU affinity : Core 0 (évite conflit avec BLE sur Core 1)

### Performance
- Frame rate animations : 30 FPS (33ms par frame)
- Interpolation en temps réel entre keyframes
- Arrêt propre des animations existantes avant d'en démarrer de nouvelles

### Gestion des erreurs
- Validation des paramètres d'entrée
- Vérification des handles de strips
- Logs détaillés pour debugging
- Retour de codes d'erreur ESP-IDF standard

## Exemple de commande

### Statique (1 couleur uniforme)
```c
van_command_t cmd = {
    .type = COMMAND_TYPE_LED,
    .command.led_cmd = {
        .led_type = LED_STATIC,
        .command.static_cmd = {
            .strip_target = ROOF_LED_ALL,
            .colors.roof.roof1_colors.color[0] = {255, 0, 0, 0, 255}, // Rouge
            // ... pour chaque LED
        }
    }
};
```

### Dynamique (animation sunrise)
```c
van_command_t cmd = {
    .type = COMMAND_TYPE_LED,
    .command.led_cmd = {
        .led_type = LED_DYNAMIC,
        .command.dynamic_cmd = {
            .strip_target = ROOF_LED_ALL_DYNAMIC,
            .loop_duration_ms = 5000,
            .keyframe_count = 3,
            .loop_behavior = LOOP_BEHAVIOR_ONCE,
            .keyframes = {
                // Keyframe 0 : Noir (0ms)
                // Keyframe 1 : Orange (2500ms)
                // Keyframe 2 : Blanc (5000ms)
            }
        }
    }
};
```

## Notes importantes

1. **Thread-safety** : Les animations tournent dans des tasks séparées
2. **Arrêt propre** : Toujours arrêter l'animation précédente avant d'en lancer une nouvelle
3. **Exterior power** : Automatiquement activé quand on contrôle les LEDs extérieures
4. **Interpolation** : Calcul en temps réel pour des transitions fluides

## Dépendances

- `led_manager.h` : Gestion bas niveau des strips
- `led_static_modes.h` : Modes statiques prédéfinis
- `led_dynamic_modes.h` : Animations prédéfinies
- `protocol.h` : Structures de commandes
- FreeRTOS : Tasks et synchronisation
