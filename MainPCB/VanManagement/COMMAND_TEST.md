# Test des Commandes - Van Management

## Commandes disponibles

Voici les commandes JSON que vous pouvez envoyer via BLE pour tester :

### 🔥 Commandes Chauffage

**Allumer le chauffage :**
```json
{"type":"command","cmd":"set_heater_state","target":0,"value":1}
```

**Éteindre le chauffage :**
```json
{"type":"command","cmd":"set_heater_state","target":0,"value":0}
```

**Définir température cible eau (65°C) :**
```json
{"type":"command","cmd":"set_heater_target","target":0,"value":650}
```
*Note: valeur = température × 10*

**Définir température cible cabine (22°C) :**
```json
{"type":"command","cmd":"set_heater_target","target":1,"value":220}
```

### 💡 Commandes LED

**Allumer LEDs toit :**
```json
{"type":"command","cmd":"set_led_state","target":0,"value":1}
```

**Éteindre LEDs toit :**
```json
{"type":"command","cmd":"set_led_state","target":0,"value":0}
```

**Allumer LEDs extérieur :**
```json
{"type":"command","cmd":"set_led_state","target":1,"value":1}
```

**Changer mode LED toit (mode 2) :**
```json
{"type":"command","cmd":"set_led_mode","target":0,"value":2}
```

**Changer luminosité LED toit (50%) :**
```json
{"type":"command","cmd":"set_led_brightness","target":0,"value":128}
```
*Note: 0-255, où 255 = 100%*

## Test de Séquence

1. **Démarrer le système** → Vérifier que l'uptime augmente
2. **Allumer chauffage** → `heater_on` doit passer à `true`
3. **Définir température** → `target_water_temp` doit changer
4. **Allumer LEDs toit** → `leds.roof.enabled` doit passer à `true`
5. **Changer luminosité** → `leds.roof.brightness` doit changer

## Réponses attendues

**Succès :**
```json
{"type":"response","status":"ok","message":"Command executed","timestamp":123456}
```

**Erreur :**
```json
{"type":"response","status":"error","message":"Invalid command format","timestamp":123456}
```

## Vérification d'État

Après chaque commande, vérifiez le JSON d'état pour confirmer que les valeurs ont changé :

- `data.heater.heater_on`
- `data.heater.target_water_temp` 
- `data.leds.roof.enabled`
- `data.leds.roof.brightness`
- etc.

## Logs ESP32

Surveillez les logs ESP32 pour voir :
```
I (123456) PROTOCOL: Heater turned ON
I (123457) PROTOCOL: LED roof state set to ON
I (123458) COMM_PROTOCOL: Command executed
```
