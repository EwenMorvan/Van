# Commandes BLE - Vidéoprojecteur Motorisé

## Informations de connexion

**Nom du device BLE** : `VideoProjector_Van`

**Service UUID** : `0x181A`

**Caractéristique de contrôle (Write)** : `0x2A58`

**Caractéristique de statut (Read/Notify)** : `0x2A19`

---

## Liste des commandes

Envoyer un **octet unique** à la caractéristique de contrôle `0x2A58` :

### Commandes de base

| Commande | Valeur (décimal) | Valeur (hex) | Description |
|----------|------------------|--------------|-------------|
| `DEPLOY` | 0 | 0x00 | Déploie complètement le vidéoprojecteur |
| `RETRACT` | 1 | 0x01 | Rétracte complètement le vidéoprojecteur |
| `STOP` | 2 | 0x02 | Arrête immédiatement le moteur |
| `GET_STATUS` | 3 | 0x03 | Demande le statut actuel (déployé/rétracté) |

### Commandes de réglage fin (JOG)




| Commande | Valeur (décimal) | Valeur (hex) | Description |
|----------|------------------|--------------|-------------|
| `JOG_UP_1` | 4 | 0x04 | Avance de **1.0 tour** de tige (vers le haut) |
| `JOG_UP_01` | 5 | 0x05 | Avance de **0.1 tour** de tige (vers le haut) |
| `JOG_UP_001` | 6 | 0x06 | Avance de **0.01 tour** de tige (vers le haut) |
| `JOG_DOWN_1` | 7 | 0x07 | Recule de **1.0 tour** de tige (vers le bas) |
| `JOG_DOWN_01` | 8 | 0x08 | Recule de **0.1 tour** de tige (vers le bas) |
| `JOG_DOWN_001` | 9 | 0x09 | Recule de **0.01 tour** de tige (vers le bas) |
| `JOG_UP_UNLIMITED` | 10 | 0x0A | Avance de **1.0 tour** de tige sans limite (dépasse 100%) |
| `JOG_DOWN_UNLIMITED` | 11 | 0x0B | Recule de **1.0 tour** de tige sans limite (dépasse 0%) |

### Commandes de calibration

| Commande | Valeur (décimal) | Valeur (hex) | Description |
|----------|------------------|--------------|-------------|
| `CALIB_UP` | 12 | 0x0C | Force la position à **100%** sans bouger le moteur |
| `CALIB_DOWN` | 13 | 0x0D | Force la position à **0%** sans bouger le moteur |




---