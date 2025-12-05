package com.van.management.ble;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import com.google.gson.Gson;
import com.van.management.data.VanCommand;

import java.util.UUID;

public class VanBleService extends Service {
    private static final String TAG = "VanBleService";
    
    // ===== CONFIGURATION APPAREIL BLE =====
    // Nom de l'appareil BLE à rechercher (modifier selon votre appareil)
    private static final String TARGET_DEVICE_NAME = "VanManager";
    
    // UUIDs du service van
    private static final String VAN_SERVICE_UUID = "0000AAA0-0000-1000-8000-00805F9B34FB";
    private static final String VAN_COMMAND_CHAR_UUID = "0000AAA1-0000-1000-8000-00805F9B34FB";
    private static final String VAN_STATE_CHAR_UUID = "0000AAA2-0000-1000-8000-00805F9B34FB";
    private static final String CLIENT_CONFIG_DESCRIPTOR_UUID = "00002902-0000-1000-8000-00805F9B34FB";
    
    // Actions d'Intent
    public static final String ACTION_GATT_CONNECTED = "com.van.management.ACTION_GATT_CONNECTED";
    public static final String ACTION_GATT_DISCONNECTED = "com.van.management.ACTION_GATT_DISCONNECTED";
    public static final String ACTION_GATT_SERVICES_DISCOVERED = "com.van.management.ACTION_GATT_SERVICES_DISCOVERED";
    public static final String ACTION_DATA_AVAILABLE = "com.van.management.ACTION_DATA_AVAILABLE";
    public static final String ACTION_COMMAND_RESPONSE = "com.van.management.ACTION_COMMAND_RESPONSE";
    public static final String EXTRA_DATA = "com.van.management.EXTRA_DATA";
    
    // Interface pour callback direct
    public interface VanBleCallback {
        void onDataReceived(String jsonData);
        void onConnectionStateChanged(boolean connected);
        void onServicesDiscovered();
        void onRssiUpdated(int rssi);
    }
    
    private BluetoothManager bluetoothManager;
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner bluetoothLeScanner;
    private BluetoothGatt bluetoothGatt;
    private BluetoothGattCharacteristic commandCharacteristic;
    private BluetoothGattCharacteristic stateCharacteristic;
    
    private Gson gson;
    private Handler mainHandler;
    private boolean isScanning = false;
    private boolean isConnected = false;
    private BleFragmentManager fragmentManager;
    private int currentMtu = 23; // MTU par défaut avant négociation
    private boolean isCleaningUp = false; // Flag pour éviter les doubles nettoyages
    private boolean isConnecting = false; // Flag pour éviter les connexions multiples
    
    // Buffer pour les données fragmentées
    private StringBuilder dataBuffer = new StringBuilder();
    
    // Callback direct pour contourner les problèmes de broadcast
    private VanBleCallback callback;
    
    // Variables pour la surveillance de connexion
    private long lastDataReceivedTime = 0;
    private static final long CONNECTION_TIMEOUT_MS = 10000; // 10 secondes sans données = déconnexion
    private static final long GATT_CHECK_INTERVAL_MS = 3000; // Vérifier la connexion GATT toutes les 3 secondes
    private Runnable connectionWatchdog;
    private Handler watchdogHandler;
    
    // Variables pour la mise à jour périodique du RSSI
    private static final long RSSI_UPDATE_INTERVAL_MS = 5000; // Mettre à jour le RSSI toutes les 5 secondes
    private Runnable rssiWatchdog;
    private Handler rssiHandler;
    
    // Variables pour les informations du device
    private String deviceName = "Unknown";
    private String deviceMacAddress = "00:00:00:00:00:00";
    private int deviceRssi = 0;
    private java.util.List<String> discoveredServices = new java.util.ArrayList<>();
    private boolean servicesDiscovered = false;
    
    public class LocalBinder extends Binder {
        public VanBleService getService() {
            return VanBleService.this;
        }
    }
    
    public void setCallback(VanBleCallback callback) {
        this.callback = callback;
        Log.d(TAG, "Callback défini: " + (callback != null ? "oui" : "non"));
    }
    
    // Getters pour les informations du device
    public String getDeviceName() {
        return deviceName;
    }
    
    public String getDeviceMacAddress() {
        return deviceMacAddress;
    }
    
    public int getDeviceRssi() {
        return deviceRssi;
    }
    
    public java.util.List<String> getDiscoveredServices() {
        return discoveredServices;
    }
    
    public boolean areServicesDiscovered() {
        return servicesDiscovered;
    }
    
    @Override
    public void onCreate() {
        super.onCreate();
        bluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        bluetoothAdapter = bluetoothManager.getAdapter();
        bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();
        gson = new Gson();
        mainHandler = new Handler(Looper.getMainLooper());
        watchdogHandler = new Handler(Looper.getMainLooper());
        rssiHandler = new Handler(Looper.getMainLooper());
        // Utiliser 276 (MTU safe pour setValue) au lieu de 512
        fragmentManager = new BleFragmentManager(276);
        
        Log.d(TAG, "Service BLE créé");
    }
    
    @Override
    public IBinder onBind(Intent intent) {
        return new LocalBinder();
    }
    
    // Callback pour le scan BLE
    private final ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            BluetoothDevice device = result.getDevice();
            String deviceName = null;
            
            try {
                deviceName = device.getName();
            } catch (SecurityException e) {
                Log.e(TAG, "Permission manquante pour obtenir le nom du dispositif: " + e.getMessage());
                return;
            }
            
            // Logger tous les appareils détectés
            Log.d(TAG, "Dispositif BLE: " + 
                  (deviceName != null ? deviceName : "[Sans nom]") + 
                  " | MAC: " + device.getAddress() + 
                  " | RSSI: " + result.getRssi() + " dBm");
            
            // Vérifier si c'est l'appareil cible ET qu'on n'est pas déjà en train de se connecter
            if (device != null && TARGET_DEVICE_NAME.equals(deviceName)) {
                // CRITIQUE : Vérifier qu'on n'est pas déjà connecté ou en train de se connecter
                if (isConnected || isConnecting) {
                    Log.w(TAG, "Appareil cible trouvé mais déjà connecté/en connexion, ignoré");
                    return;
                }
                
                Log.i(TAG, ">>> APPAREIL CIBLE TROUVÉ: " + deviceName + " (" + device.getAddress() + ")");
                
                // Sauvegarder les infos du device
                VanBleService.this.deviceName = deviceName;
                VanBleService.this.deviceMacAddress = device.getAddress();
                VanBleService.this.deviceRssi = result.getRssi();
                
                // Arrêter le scan IMMÉDIATEMENT (avant de marquer isConnecting)
                stopScan();
                
                // Lancer la connexion (qui marquera isConnecting = true)
                connectToDevice(device);
            }
        }
        
        @Override
        public void onScanFailed(int errorCode) {
            String errorMsg;
            switch (errorCode) {
                case SCAN_FAILED_ALREADY_STARTED:
                    errorMsg = "Scan déjà démarré";
                    break;
                case SCAN_FAILED_APPLICATION_REGISTRATION_FAILED:
                    errorMsg = "Échec d'enregistrement de l'application";
                    break;
                case SCAN_FAILED_FEATURE_UNSUPPORTED:
                    errorMsg = "Fonctionnalité non supportée";
                    break;
                case SCAN_FAILED_INTERNAL_ERROR:
                    errorMsg = "Erreur interne";
                    break;
                default:
                    errorMsg = "Erreur inconnue";
                    break;
            }
            Log.e(TAG, "Échec du scan BLE - Code: " + errorCode + " (" + errorMsg + ")");
            isScanning = false;
        }
    };
    
    // Callback GATT
    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    Log.d(TAG, "Connecté au serveur GATT");
                    isConnected = true;
                    isConnecting = false; // Connexion réussie, réinitialiser le flag
                    lastDataReceivedTime = System.currentTimeMillis();
                    startConnectionWatchdog();
                    
                    // Réinitialiser le buffer et les caractéristiques
                    dataBuffer.setLength(0);
                    commandCharacteristic = null;
                    stateCharacteristic = null;
                    
                    broadcastUpdate(ACTION_GATT_CONNECTED);
                    
                    // Callback direct
                    if (callback != null) {
                        mainHandler.post(() -> callback.onConnectionStateChanged(true));
                    }
                    
                    // Demander un MTU plus grand avec un délai
                    mainHandler.postDelayed(() -> {
                        if (bluetoothGatt != null && isConnected) {
                            Log.d(TAG, "Demande de MTU...");
                            boolean mtuResult = gatt.requestMtu(512);
                            Log.d(TAG, "Résultat demande MTU: " + mtuResult);
                            
                            if (!mtuResult) {
                                // Si la demande de MTU échoue, lancer directement la découverte des services
                                Log.w(TAG, "Échec demande MTU, lancement découverte services...");
                                mainHandler.postDelayed(() -> {
                                    if (bluetoothGatt != null && isConnected) {
                                        boolean discoverResult = gatt.discoverServices();
                                        Log.d(TAG, "Résultat découverte services (après échec MTU): " + discoverResult);
                                    }
                                }, 300);
                            }
                        }
                    }, 500);
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    Log.d(TAG, "Déconnecté du serveur GATT");
                    isConnected = false;
                    isConnecting = false; // Réinitialiser le flag
                    stopConnectionWatchdog();
                    
                    // Nettoyer les données
                    dataBuffer.setLength(0);
                    commandCharacteristic = null;
                    stateCharacteristic = null;
                    
                    broadcastUpdate(ACTION_GATT_DISCONNECTED);
                    
                    // Callback direct
                    if (callback != null) {
                        mainHandler.post(() -> callback.onConnectionStateChanged(false));
                    }
                }
            } else {
                Log.e(TAG, "Erreur de connexion GATT - status: " + status + ", newState: " + newState);
                
                // Nettoyer complètement en cas d'erreur
                new Thread(() -> {
                    cleanupGattConnection();
                    
                    // Notifier l'erreur
                    mainHandler.post(() -> {
                        broadcastUpdate(ACTION_GATT_DISCONNECTED);
                        
                        // Callback direct pour les erreurs aussi
                        if (callback != null) {
                            callback.onConnectionStateChanged(false);
                        }
                    });
                }).start();
            }
        }
        
        @Override
        public void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                currentMtu = mtu;
                Log.d(TAG, "✅ MTU changé à: " + mtu + " (effectif: " + (mtu - 3) + " bytes)");
                
                // IMPORTANT: setValue() est limité à ~273 bytes même avec MTU=512
                // On utilise donc un MTU safe de 276 pour éviter la troncature
                int safeMtu = Math.min(mtu, 276);
                Log.d(TAG, "📦 Utilisation MTU safe: " + safeMtu + " (pour éviter troncature setValue)");
                
                // Recréer le fragment manager avec le MTU safe
                fragmentManager = new BleFragmentManager(safeMtu);
            } else {
                Log.w(TAG, "⚠️ Échec changement MTU, status: " + status + ", utilisation MTU par défaut (23)");
                currentMtu = 23;
            }
            
            // Toujours découvrir les services après changement de MTU (ou après échec)
            // Petit délai pour s'assurer que tout est stable
            mainHandler.postDelayed(() -> {
                if (bluetoothGatt != null && isConnected) {
                    Log.d(TAG, "Lancement découverte des services...");
                    boolean success = gatt.discoverServices();
                    Log.d(TAG, "Résultat découverte services: " + success);
                    
                    if (!success) {
                        Log.e(TAG, "❌ Échec de la découverte des services");
                    }
                }
            }, 200);
        }
        
        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            Log.d(TAG, "=== onServicesDiscovered appelé - status: " + status + " ===");
            
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "✅ Services découverts avec succès");
                
                // Sauvegarder les services découverts
                discoveredServices.clear();
                for (BluetoothGattService service : gatt.getServices()) {
                    String serviceUuid = service.getUuid().toString();
                    discoveredServices.add(serviceUuid);
                    Log.d(TAG, "📋 Service disponible: " + serviceUuid);
                }
                servicesDiscovered = true;
                Log.d(TAG, "📊 Total services découverts: " + discoveredServices.size());
                
                BluetoothGattService vanService = gatt.getService(UUID.fromString(VAN_SERVICE_UUID));
                if (vanService != null) {
                    Log.d(TAG, "Service van trouvé");
                    commandCharacteristic = vanService.getCharacteristic(UUID.fromString(VAN_COMMAND_CHAR_UUID));
                    stateCharacteristic = vanService.getCharacteristic(UUID.fromString(VAN_STATE_CHAR_UUID));
                    
                    if (commandCharacteristic != null) {
                        Log.d(TAG, "Caractéristique de commande trouvée");
                    } else {
                        Log.e(TAG, "Caractéristique de commande non trouvée");
                    }
                    
                    // Activer les notifications pour les états
                    if (stateCharacteristic != null) {
                        Log.d(TAG, "Caractéristique d'état trouvée, activation des notifications");
                        boolean notifResult = gatt.setCharacteristicNotification(stateCharacteristic, true);
                        Log.d(TAG, "Notification activée: " + notifResult);
                        
                        BluetoothGattDescriptor descriptor = stateCharacteristic.getDescriptor(
                            UUID.fromString(CLIENT_CONFIG_DESCRIPTOR_UUID));
                        if (descriptor != null) {
                            descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                            boolean writeResult = gatt.writeDescriptor(descriptor);
                            Log.d(TAG, "Écriture descripteur: " + writeResult);
                        } else {
                            Log.e(TAG, "Descripteur de configuration client non trouvé");
                        }
                    } else {
                        Log.e(TAG, "Caractéristique d'état non trouvée");
                    }
                    
                    broadcastUpdate(ACTION_GATT_SERVICES_DISCOVERED);
                    
                    // Callback direct
                    if (callback != null) {
                        mainHandler.post(() -> callback.onServicesDiscovered());
                    }
                    
                    // Lire le RSSI maintenant que tout est prêt et démarrer les mises à jour périodiques
                    mainHandler.postDelayed(() -> {
                        if (bluetoothGatt != null && isConnected) {
                            Log.d(TAG, "📶 Lecture RSSI initial...");
                            boolean rssiResult = gatt.readRemoteRssi();
                            Log.d(TAG, "Résultat lecture RSSI: " + rssiResult);
                            
                            // Démarrer les mises à jour périodiques du RSSI
                            startRssiWatchdog();
                        }
                    }, 300);
                } else {
                    Log.e(TAG, "❌ Service van non trouvé - UUID: " + VAN_SERVICE_UUID);
                    // Lister tous les services disponibles pour debug
                    for (BluetoothGattService service : gatt.getServices()) {
                        Log.e(TAG, "❓ Service disponible: " + service.getUuid());
                    }
                }
            } else {
                Log.e(TAG, "❌ Échec de la découverte des services - status: " + status);
            }
        }
        
        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
            // Notification reçue (état du van)
            Log.d(TAG, "onCharacteristicChanged appelé, UUID: " + characteristic.getUuid().toString().toUpperCase());
            
            if (VAN_STATE_CHAR_UUID.equals(characteristic.getUuid().toString().toUpperCase())) {
                // Mettre à jour le timestamp de dernière réception de données
                lastDataReceivedTime = System.currentTimeMillis();
                
                String fragment = new String(characteristic.getValue());
                Log.d(TAG, "Fragment reçu (" + fragment.length() + " chars): " + fragment.substring(0, Math.min(100, fragment.length())));
                
                // SÉCURITÉ 1 : Si le fragment commence par le marqueur de début, vider le buffer
                if (fragment.startsWith("{\"start_van_state\":\"\"")) {
                    if (dataBuffer.length() > 0) {
                        Log.w(TAG, "⚠️ Nouveau JSON détecté alors que le buffer n'est pas vide (" + dataBuffer.length() + " chars), nettoyage...");
                        dataBuffer.setLength(0);
                    }
                    Log.d(TAG, "🆕 Début de nouveau JSON détecté");
                }
                
                // Ajouter le fragment au buffer
                dataBuffer.append(fragment);
                
                // SÉCURITÉ 2 : Limiter la taille du buffer à 200 Ko max (sécurité contre accumulation)
                if (dataBuffer.length() > 200000) {
                    Log.e(TAG, "❌ Buffer trop grand (" + dataBuffer.length() + " chars), nettoyage forcé !");
                    dataBuffer.setLength(0);
                    return;
                }
                
                // Vérifier si on a un JSON complet
                String bufferContent = dataBuffer.toString();
                
                // SÉCURITÉ 3 : Vérifier que le buffer commence bien par le marqueur de début
                if (bufferContent.length() > 100 && !bufferContent.startsWith("{\"start_van_state\":\"\"")) {
                    Log.e(TAG, "❌ Buffer corrompu (ne commence pas par start_van_state), nettoyage...");
                    dataBuffer.setLength(0);
                    return;
                }
                
                Log.d(TAG, "Buffer actuel: " + bufferContent.length() + " chars, début OK: " + 
                      bufferContent.startsWith("{\"start_van_state\":\"\"") + ", fin OK: " + 
                      bufferContent.endsWith("\"end_van_state\":\"\"}\n"));
                
                // Vérifier si le JSON est complet
                if (bufferContent.startsWith("{\"start_van_state\":\"\"") && bufferContent.endsWith("\"end_van_state\":\"\"}\n")) {
                    Log.d(TAG, "✅ JSON complet reçu (" + bufferContent.length() + " chars)");
                    //Log.d(TAG, "JSON reçu : " + bufferContent.toString());

                    if (callback != null) {
                        mainHandler.post(() -> callback.onDataReceived(bufferContent));
                    } else {
                        Log.e(TAG, "Callback est null ! Impossible d'envoyer les données");
                    }
                    
                    // Vider le buffer
                    dataBuffer.setLength(0);
                } else {
                    Log.d(TAG, "📦 JSON incomplet, attente de plus de données... (" + bufferContent.length() + " chars actuellement)");
                }
            }
        }
        
        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "Commande envoyée avec succès");
            } else {
                Log.e(TAG, "Échec d'envoi de commande: " + status);
            }
        }
        
        @Override
        public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "✅ Descripteur écrit avec succès - UUID: " + descriptor.getUuid() + " - Les notifications sont maintenant ACTIVES");
            } else {
                Log.e(TAG, "❌ Échec écriture descripteur - status: " + status + " - UUID: " + descriptor.getUuid());
            }
        }
        
        @Override
        public void onReadRemoteRssi(BluetoothGatt gatt, int rssi, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                deviceRssi = rssi;
                Log.d(TAG, "📶 RSSI mis à jour: " + rssi + " dBm");
                
                // Notifier via callback
                if (callback != null) {
                    mainHandler.post(() -> callback.onRssiUpdated(rssi));
                }
            } else {
                Log.w(TAG, "⚠️ Échec lecture RSSI - status: " + status);
            }
        }
    };
    
    public void startScan() {
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
            Log.e(TAG, "Bluetooth non disponible ou désactivé");
            return;
        }
        
        if (bluetoothLeScanner == null) {
            Log.e(TAG, "Scanner BLE non disponible");
            return;
        }
        
        if (isScanning) {
            Log.d(TAG, "Scan déjà en cours");
            return;
        }
        
        // Ne pas scanner si déjà connecté ou en connexion
        if (isConnected || isConnecting) {
            Log.w(TAG, "Scan ignoré : déjà connecté ou en connexion");
            return;
        }
        
        // S'assurer que toute connexion précédente est fermée AVANT de scanner
        if (bluetoothGatt != null) {
            Log.w(TAG, "GATT existant détecté avant scan, nettoyage forcé...");
            cleanupGattConnection();
        }
        
        // Réinitialiser le flag de connexion au cas où
        isConnecting = false;
        
        try {
            ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build();
            
            // Scanner SANS filtre pour voir tous les appareils
            // Le filtrage se fera dans le callback
            bluetoothLeScanner.startScan(null, settings, scanCallback);
            isScanning = true;
            Log.d(TAG, "Scan BLE démarré - Recherche de l'appareil: " + TARGET_DEVICE_NAME);
            
            // Arrêter le scan après 10 secondes
            mainHandler.postDelayed(this::stopScan, 10000);
        } catch (SecurityException e) {
            Log.e(TAG, "Permission manquante pour le scan BLE: " + e.getMessage());
            isScanning = false;
        } catch (Exception e) {
            Log.e(TAG, "Erreur lors du démarrage du scan BLE: " + e.getMessage());
            isScanning = false;
        }
    }
    
    public void stopScan() {
        if (bluetoothLeScanner != null && isScanning) {
            try {
                bluetoothLeScanner.stopScan(scanCallback);
                Log.d(TAG, "Scan BLE arrêté");
            } catch (SecurityException e) {
                Log.e(TAG, "Permission manquante pour arrêter le scan BLE: " + e.getMessage());
            } catch (Exception e) {
                Log.e(TAG, "Erreur lors de l'arrêt du scan BLE: " + e.getMessage());
            } finally {
                isScanning = false;
            }
        }
    }
    
    /**
     * Nettoyage complet et synchrone de la connexion GATT
     * Cette méthode garantit qu'aucune connexion fantôme ne reste en mémoire
     */
    private void cleanupGattConnection() {
        if (isCleaningUp) {
            Log.w(TAG, "Nettoyage déjà en cours, skip...");
            return;
        }
        
        isCleaningUp = true;
        Log.d(TAG, "🧹 DÉBUT NETTOYAGE COMPLET GATT");
        
        // Arrêter les watchdogs
        stopConnectionWatchdog();
        stopRssiWatchdog();
        
        // Marquer comme déconnecté
        isConnected = false;
        isConnecting = false; // Réinitialiser le flag de connexion
        
        // Nettoyer les données
        commandCharacteristic = null;
        stateCharacteristic = null;
        dataBuffer.setLength(0);
        
        // Fermer la connexion GATT si elle existe
        if (bluetoothGatt != null) {
            try {
                // Désactiver les notifications d'abord
                if (stateCharacteristic != null) {
                    bluetoothGatt.setCharacteristicNotification(stateCharacteristic, false);
                }
                
                // Déconnecter
                bluetoothGatt.disconnect();
                
                // Attendre un peu pour que la déconnexion soit effective
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    // Ignorer
                }
                
                // Fermer
                bluetoothGatt.close();
                Log.d(TAG, "✅ GATT fermé");
            } catch (Exception e) {
                Log.e(TAG, "Erreur lors du nettoyage GATT: " + e.getMessage());
            } finally {
                bluetoothGatt = null;
            }
        }
        
        // Attendre un peu plus pour que le système libère les ressources
        try {
            Thread.sleep(200);
        } catch (InterruptedException e) {
            // Ignorer
        }
        
        Log.d(TAG, "🧹 FIN NETTOYAGE COMPLET GATT");
        isCleaningUp = false;
    }
    
    private void connectToDevice(BluetoothDevice device) {
        Log.d(TAG, "connectToDevice() appelé - isConnecting=" + isConnecting + ", isConnected=" + isConnected);
        
        // SÉCURITÉ : Ne jamais créer plusieurs connexions en parallèle
        if (isConnecting || isConnected) {
            Log.w(TAG, "Tentative de connexion ignorée (déjà en cours ou connecté)");
            return;
        }
        
        // Marquer IMMÉDIATEMENT comme "en connexion" pour bloquer les appels suivants
        isConnecting = true;
        Log.d(TAG, "isConnecting mis à TRUE");
        
        // Nettoyage complet et synchrone avant toute nouvelle connexion
        if (bluetoothGatt != null) {
            Log.d(TAG, "Fermeture de la connexion GATT existante avant reconnexion");
            cleanupGattConnection();
        }
        
        Log.d(TAG, "Tentative de connexion au dispositif: " + device.getAddress());
        
        // Délai pour permettre au système de libérer les ressources
        mainHandler.postDelayed(() -> {
            try {
                Log.d(TAG, "Création d'une nouvelle connexion GATT...");
                bluetoothGatt = device.connectGatt(this, false, gattCallback, BluetoothDevice.TRANSPORT_LE);
                if (bluetoothGatt == null) {
                    Log.e(TAG, "Échec de création de la connexion GATT");
                    isConnecting = false; // Réinitialiser le flag
                    broadcastUpdate(ACTION_GATT_DISCONNECTED);
                    
                    if (callback != null) {
                        mainHandler.post(() -> callback.onConnectionStateChanged(false));
                    }
                }
            } catch (SecurityException e) {
                Log.e(TAG, "Permission manquante pour la connexion BLE: " + e.getMessage());
                isConnecting = false; // Réinitialiser le flag
                broadcastUpdate(ACTION_GATT_DISCONNECTED);
                
                if (callback != null) {
                    mainHandler.post(() -> callback.onConnectionStateChanged(false));
                }
            }
        }, 800); // Délai augmenté pour laisser le temps au système
    }
    
    public void disconnect() {
        Log.d(TAG, "Déconnexion demandée");
        
        // Utiliser la méthode de nettoyage complète
        new Thread(() -> {
            cleanupGattConnection();
            
            // Notifier après le nettoyage
            mainHandler.post(() -> {
                broadcastUpdate(ACTION_GATT_DISCONNECTED);
                if (callback != null) {
                    callback.onConnectionStateChanged(false);
                }
            });
        }).start();
    }

    
    /**
     * Envoie une commande binaire structurée à l'ESP32
     * Gère automatiquement la fragmentation si la commande dépasse le MTU
     * @param command La commande VanCommand à envoyer
     * @return true si l'envoi a été lancé avec succès
     */
    public boolean sendBinaryCommand(VanCommand command) {
        if (commandCharacteristic == null || !isConnected) {
            Log.e(TAG, "Pas de connexion ou caractéristique manquante");
            return false;
        }
        
        byte[] commandBytes = command.toBytes();
        Log.d(TAG, "📤 Envoi commande: " + command.getType() + " (" + commandBytes.length + " bytes)");
        Log.d(TAG, "📡 MTU actuel: " + currentMtu + " (effectif: " + (currentMtu - 3) + " bytes)");
        
        // Calculer les statistiques de fragmentation
        BleFragmentManager.FragmentStats stats = fragmentManager.calculateStats(commandBytes.length);
        Log.d(TAG, "📊 " + stats.toString());
        
        // Fragmenter les données
        byte[][] fragments = fragmentManager.fragmentData(commandBytes);
        
        if (fragments.length == 1) {
            // Envoi direct (pas de fragmentation)
            Log.d(TAG, "✅ Envoi direct (pas de fragmentation)");
            commandCharacteristic.setValue(fragments[0]);
            boolean result = bluetoothGatt.writeCharacteristic(commandCharacteristic);
            
            if (result) {
                Log.d(TAG, "✅ Commande envoyée avec succès");
            } else {
                Log.e(TAG, "❌ Échec d'envoi");
            }
            
            return result;
        } else {
            // Envoi fragmenté
            Log.d(TAG, "📦 Fragmentation en " + fragments.length + " paquets");
            return sendFragmentedCommand(fragments);
        }
    }
    
    /**
     * Envoie une commande fragmentée
     */
    private boolean sendFragmentedCommand(byte[][] fragments) {
        Log.d(TAG, "🔄 Début envoi fragmenté (" + fragments.length + " fragments)");
        
        // Envoyer chaque fragment avec un délai entre chaque
        for (int i = 0; i < fragments.length; i++) {
            final int index = i;
            final byte[] fragment = fragments[i];
            
            // Délai entre les fragments (sauf pour le premier)
            if (i > 0) {
                try {
                    Thread.sleep(200); // 1ms entre chaque fragment
                } catch (InterruptedException e) {
                    Log.e(TAG, "Interruption pendant l'envoi fragmenté", e);
                    return false;
                }
            }
            
            Log.d(TAG, String.format("📤 Fragment %d/%d (%d bytes avant setValue)", 
                index + 1, fragments.length, fragment.length));
            
            // IMPORTANT: setValue peut tronquer, on vérifie après
            commandCharacteristic.setValue(fragment);
            byte[] actualValue = commandCharacteristic.getValue();
            
            if (actualValue.length != fragment.length) {
                Log.e(TAG, String.format("⚠️ ATTENTION: setValue a tronqué: %d -> %d bytes", 
                    fragment.length, actualValue.length));
                Log.e(TAG, "Ceci indique que le MTU n'est pas assez grand ou pas encore négocié");
            }
            
            boolean result = bluetoothGatt.writeCharacteristic(commandCharacteristic);
            
            if (!result) {
                Log.e(TAG, "❌ Échec envoi fragment " + (index + 1) + "/" + fragments.length);
                return false;
            }
            
            Log.d(TAG, String.format("✅ Fragment %d/%d envoyé (%d bytes effectifs)", 
                index + 1, fragments.length, actualValue.length));
        }
        
        Log.d(TAG, "✅ Tous les fragments envoyés avec succès");
        return true;
    }
    
    public boolean isConnected() {
        return isConnected;
    }
    
    public boolean isScanning() {
        return isScanning;
    }
    
    public int getCurrentMtu() {
        return currentMtu;
    }
    
    public boolean isReadyForLargeCommands() {
        return isConnected && currentMtu > 23;
    }

    
    private void broadcastUpdate(String action) {
        Intent intent = new Intent(action);
        Log.d(TAG, "Envoi broadcast: " + action);
        sendBroadcast(intent);
    }
    
    private void broadcastUpdate(String action, String data) {
        Intent intent = new Intent(action);
        intent.putExtra(EXTRA_DATA, data);
        Log.d(TAG, "Envoi broadcast: " + action + " avec données de longueur: " + (data != null ? data.length() : "null"));
        sendBroadcast(intent);
    }
    
    /**
     * Démarre la surveillance de la connexion pour détecter les déconnexions silencieuses
     */
    private void startConnectionWatchdog() {
        Log.d(TAG, "=== DÉMARRAGE WATCHDOG DE CONNEXION ===");
        stopConnectionWatchdog(); // Arrêter le précédent s'il existe
        
        connectionWatchdog = new Runnable() {
            @Override
            public void run() {
                Log.d(TAG, "Watchdog exécuté - isConnected: " + isConnected + ", bluetoothGatt: " + (bluetoothGatt != null));
                
                if (isConnected) {
                    long timeSinceLastData = System.currentTimeMillis() - lastDataReceivedTime;
                    Log.d(TAG, "Watchdog: dernières données reçues il y a " + timeSinceLastData + "ms (timeout: " + CONNECTION_TIMEOUT_MS + "ms)");
                    
                    boolean shouldDisconnect = false;
                    String reason = "";
                    
                    // Vérification principale : données reçues récemment ?
                    if (timeSinceLastData > CONNECTION_TIMEOUT_MS) {
                        shouldDisconnect = true;
                        reason = "Timeout de données (" + timeSinceLastData + "ms)";
                    }
                    
                    // Vérification secondaire : état GATT
                    if (bluetoothGatt != null) {
                        try {
                            BluetoothManager bluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
                            BluetoothDevice device = bluetoothGatt.getDevice();
                            int connectionState = bluetoothManager.getConnectionState(device, BluetoothProfile.GATT);
                            
                            Log.d(TAG, "État GATT: " + connectionState + " (STATE_CONNECTED=" + BluetoothProfile.STATE_CONNECTED + ")");
                            
                            if (connectionState != BluetoothProfile.STATE_CONNECTED) {
                                shouldDisconnect = true;
                                reason = "État GATT déconnecté (" + connectionState + ")";
                            }
                        } catch (Exception e) {
                            Log.e(TAG, "Erreur lors de la vérification GATT: " + e.getMessage());
                            shouldDisconnect = true;
                            reason = "Erreur GATT: " + e.getMessage();
                        }
                    }
                    
                    if (shouldDisconnect) {
                        Log.w(TAG, "=== DÉCONNEXION FORCÉE DÉTECTÉE ===");
                        Log.w(TAG, "Raison: " + reason);
                        forceDisconnection();
                    } else {
                        Log.d(TAG, "Connexion OK, reprogrammation du watchdog dans 3s");
                        // Programmer la prochaine vérification
                        watchdogHandler.postDelayed(this, 3000);
                    }
                } else {
                    Log.d(TAG, "Watchdog arrêté car pas connecté");
                }
            }
        };
        
        // Première vérification dans 3 secondes
        Log.d(TAG, "Programmation première vérification watchdog dans 3s");
        watchdogHandler.postDelayed(connectionWatchdog, 3000);
    }
    
    /**
     * Arrête la surveillance de la connexion
     */
    private void stopConnectionWatchdog() {
        if (connectionWatchdog != null) {
            watchdogHandler.removeCallbacks(connectionWatchdog);
            connectionWatchdog = null;
            Log.d(TAG, "Watchdog de connexion arrêté");
        }
    }
    
    /**
     * Démarre la mise à jour périodique du RSSI
     */
    private void startRssiWatchdog() {
        Log.d(TAG, "📶 Démarrage watchdog RSSI (intervalle: " + RSSI_UPDATE_INTERVAL_MS + "ms)");
        stopRssiWatchdog(); // Arrêter le précédent s'il existe
        
        rssiWatchdog = new Runnable() {
            @Override
            public void run() {
                if (isConnected && bluetoothGatt != null) {
                    Log.d(TAG, "📶 Lecture RSSI périodique...");
                    boolean result = bluetoothGatt.readRemoteRssi();
                    if (!result) {
                        Log.w(TAG, "⚠️ Échec lecture RSSI périodique");
                    }
                    
                    // Re-programmer pour la prochaine mise à jour
                    rssiHandler.postDelayed(this, RSSI_UPDATE_INTERVAL_MS);
                } else {
                    Log.d(TAG, "📶 Watchdog RSSI arrêté (déconnecté)");
                }
            }
        };
        
        // Première lecture dans 5 secondes
        rssiHandler.postDelayed(rssiWatchdog, RSSI_UPDATE_INTERVAL_MS);
    }
    
    /**
     * Arrête la mise à jour périodique du RSSI
     */
    private void stopRssiWatchdog() {
        if (rssiWatchdog != null) {
            rssiHandler.removeCallbacks(rssiWatchdog);
            rssiWatchdog = null;
            Log.d(TAG, "Watchdog RSSI arrêté");
        }
    }
    
    /**
     * Force une déconnexion en cas de timeout détecté
     */
    private void forceDisconnection() {
        Log.w(TAG, "Déconnexion forcée suite à un timeout");
        
        // Utiliser la méthode de nettoyage complète dans un thread séparé
        new Thread(() -> {
            cleanupGattConnection();
            
            // Notifier la déconnexion
            mainHandler.post(() -> {
                broadcastUpdate(ACTION_GATT_DISCONNECTED);
                
                // Callback direct
                if (callback != null) {
                    callback.onConnectionStateChanged(false);
                }
            });
        }).start();
    }
    
    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "Destruction du service BLE...");
        
        stopConnectionWatchdog();
        stopRssiWatchdog();
        stopScan();
        
        // Nettoyage complet et synchrone
        cleanupGattConnection();
        
        // Nettoyer les handlers
        if (mainHandler != null) {
            mainHandler.removeCallbacksAndMessages(null);
        }
        if (watchdogHandler != null) {
            watchdogHandler.removeCallbacksAndMessages(null);
        }
        if (rssiHandler != null) {
            rssiHandler.removeCallbacksAndMessages(null);
        }
        
        Log.d(TAG, "Service BLE détruit");
    }
}
