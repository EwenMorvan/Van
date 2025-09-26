# Configuration OTA (Over-The-Air) pour ESP32

## 🚀 Configuration rapide

### 1. Configuration WiFi
Les paramètres WiFi se configurent via `menuconfig` ou directement dans `sdkconfig.defaults`:

```bash
make config
# Aller dans "WiFi OTA Configuration"
# Configurer SSID, mot de passe, etc.
```

### 2. Compilation et premier flash USB
```bash
make build
make flash PORT=/dev/ttyUSB0
```

### 3. Upload OTA sans fil
```bash
# Trouver l'IP de l'ESP32 (voir logs série)
make ota IP=192.168.1.100
```

## 📋 Commandes disponibles

| Commande | Description |
|----------|-------------|
| `make build` | Compiler le projet |
| `make flash` | Flash initial via USB |
| `make ota IP=x.x.x.x` | Upload OTA sans fil |
| `make monitor` | Monitorer les logs série |
| `make config` | Configuration menuconfig |
| `make clean` | Nettoyer le build |

## ⚙️ Configuration WiFi

### Via menuconfig:
```bash
make config
```
Puis aller dans **WiFi OTA Configuration** et configurer:
- SSID WiFi
- Mot de passe WiFi
- IP du serveur OTA
- Port OTA (défaut: 8070)

### Via fichier de configuration:
Éditez `sdkconfig.defaults` et ajoutez:
```
CONFIG_WIFI_SSID="votre_wifi"
CONFIG_WIFI_PASSWORD="votre_mot_de_passe"
CONFIG_OTA_SERVER_IP="192.168.1.10"
```

## 🔄 Processus OTA

1. **Premier flash USB** (obligatoire)
   ```bash
   make flash PORT=/dev/ttyUSB0
   ```

2. **L'ESP32 se connecte au WiFi**
   - Vérifiez les logs série pour voir l'IP attribuée
   - L'ESP32 démarre un serveur HTTP sur le port 8080

3. **Upload OTA depuis votre PC**
   ```bash
   make ota IP=192.168.1.100
   ```

4. **L'ESP32 redémarre automatiquement** avec le nouveau firmware

## 🌐 Interface Web OTA

L'ESP32 expose une interface web simple:
- `http://IP_ESP32:8080/` - Page d'information
- `http://IP_ESP32:8080/update` - Upload de firmware (POST)

Vous pouvez aussi uploader manuellement via curl:
```bash
curl -X POST -F "file=@build/WaterManagment.bin" http://192.168.1.100:8080/update
```

## 🛡️ Sécurité OTA

### Rollback automatique
Le système inclut un mécanisme de rollback:
- Si le nouveau firmware ne démarre pas correctement, l'ESP32 revient automatiquement à la version précédente
- Timeout de 60 secondes pour validation

### Validation du firmware
- Vérification de la taille du firmware
- Vérification de la signature (si activée)
- Vérification de compatibilité

## 🔧 Dépannage

### L'ESP32 ne se connecte pas au WiFi
1. Vérifiez le SSID et mot de passe dans la configuration
2. Vérifiez que le WiFi 2.4GHz est disponible (pas 5GHz)
3. Regardez les logs série pour les erreurs de connexion

### Upload OTA échoue
1. Vérifiez que l'ESP32 est accessible: `ping IP_ESP32`
2. Vérifiez que le port 8080 est ouvert
3. Vérifiez la taille du firmware (max ~1MB par partition)

### L'ESP32 ne redémarre pas après OTA
1. Le nouveau firmware peut avoir des erreurs
2. Le rollback automatique devrait se déclencher après 60s
3. Si bloqué, reflashez via USB

## 📁 Structure des partitions

Le fichier `partitions.csv` définit:
```
# Name,   Type, SubType, Offset,  Size
factory,  app,  factory, 0x10000, 1M     # Firmware d'usine
ota_0,    app,  ota_0,   0x110000, 1M    # Partition OTA 1
ota_1,    app,  ota_1,   0x210000, 1M    # Partition OTA 2
ota_data, data, ota,     0x310000, 0x2000 # Données OTA
```

## 🔍 Logs et monitoring

Surveillez les logs pour:
```
I (123) WIFI_OTA: WiFi connected, IP: 192.168.1.100
I (124) WIFI_OTA: OTA server started on port 8080
I (125) WIFI_OTA: Ready for OTA updates
```

En cas d'upload OTA:
```
I (456) WIFI_OTA: OTA update started...
I (789) WIFI_OTA: OTA update successful, restarting...
```

## 🚨 Points importants

1. **Premier flash obligatoire via USB** - L'OTA ne peut pas être la première méthode de flash
2. **Partition OTA requise** - Utilisez la table de partitions fournie
3. **WiFi 2.4GHz uniquement** - L'ESP32 ne supporte pas 5GHz
4. **Taille limitée** - Maximum ~1MB par firmware
5. **Rollback automatique** - Sauvegarde en cas de problème

## 📞 Support

En cas de problème:
1. Vérifiez les logs série avec `make monitor`
2. Testez la connectivité avec `make ping-esp IP=x.x.x.x`
3. En dernier recours, reflashez via USB avec `make flash`
