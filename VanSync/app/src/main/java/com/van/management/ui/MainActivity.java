package com.van.management.ui;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.renderscript.RenderScript;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.van.management.R;
import com.van.management.ble.VanBleService;
import com.van.management.data.VanCommand;
import com.van.management.data.VanState;
import com.van.management.ui.tabs.*;
import com.van.management.utils.PreferencesManager;
import com.van.management.utils.VanStateParser;


public class MainActivity extends AppCompatActivity implements VanBleService.VanBleCallback {
    private static final String TAG = "MainActivity";
    private static final int REQUEST_ENABLE_BT = 1;
    private static final int REQUEST_PERMISSIONS = 2;
    
    // ===== BLE Service =====
    private VanBleService bleService;
    private boolean serviceConnected = false;
    
    // ===== UI - Bluetooth Status =====
    private ImageView bluetoothIconBg;
    private ImageView bluetoothIcon;
    private TextView bluetoothStatusText;
    private TextView bluetoothServicesStatusText;
    private ImageView signalIcon;
    private android.view.View bleLayout;
    
    // ===== UI - System Health Status =====
    private ImageView systemHealthBg;
    private ImageView systemHealthLogo;
    private TextView systemHealthStatus;
    private TextView systemHealthUptime;
    private android.view.View systemLayout;
    
    // ===== UI - Tabs =====
    private FrameLayout contentContainer;
    private ImageButton tabButtonHome, tabButtonLight, tabButtonWater, tabButtonHeater, tabButtonEnergy, tabButtonMultimedia;
    
    // ===== UI - Warning Popup =====
    private android.view.View warningPopup;
    private TextView popupTitle;
    private TextView statusMessage;
    private TextView connectionStatusValue;
    private TextView reconnectAttemptsValue;
    private TextView actionValue;
    
    // ===== UI - Bluetooth Info Popup =====
    private android.view.View bluetoothPopup;
    private TextView popupConnectionStatus;
    private TextView popupServiceStatus;
    private TextView popupDeviceName;
    private TextView popupDeviceMac;
    private TextView popupDeviceRssi;
    private TextView popupDeviceMtu;
    private LinearLayout popupServicesContainer;
    private android.view.View popupConnectButton;
    private TextView popupConnectButtonText;
    
    // ===== UI - System Health Popup =====
    private android.view.View systemHealthPopup;
    private LinearLayout overviewContainer;
    private LinearLayout mainErrorsContainer;
    private LinearLayout slaveErrorsContainer;
    private LinearLayout vanStateContainer;
    
    // ===== UI - Loading Overlay =====
    private View loadingOverlay;
    
    private enum Tab {
        NONE, HOME, LIGHT, WATER, HEATER, ENERGY, MULTIMEDIA
    }
    
    private Tab currentTab = Tab.NONE;
    
    private int[] tabLayouts = {
        0,
        R.layout.overview_layout,
        R.layout.light_layout,
        R.layout.water_layout,
        R.layout.heater_layout,
        R.layout.energy_layout,
        R.layout.multimedia_layout
    };
    
    // ===== Cache des vues de tabs =====
    private android.util.SparseArray<View> cachedTabViews = new android.util.SparseArray<>();
    
    // ===== Tab Managers =====
    private android.util.SparseArray<BaseTab> tabManagers = new android.util.SparseArray<>();
    private HomeTab homeTab;
    private LightTab lightTab;
    private WaterTab waterTab;
    private HeaterTab heaterTab;
    private EnergyTab energyTab;
    private MultimediaTab multimediaTab;
    
    // ===== Data =====
    private PreferencesManager preferencesManager;
    private VanState lastVanState;
    
    // ===== Connection tracking =====
    private boolean hasBeenConnectedBefore = false;
    
    // ===== Auto-reconnection =====
    private Handler reconnectHandler;
    private int reconnectAttempts = 0;
    private static final int MAX_RECONNECT_ATTEMPTS = 5;
    private static final int RECONNECT_DELAY_MS = 5000; // Augmenté à 5s pour laisser le temps au BLE de se nettoyer
    private Runnable reconnectRunnable;
    
    // ===== RenderScript =====
    private RenderScript renderScript;

    
    // ==================== LIFECYCLE ====================
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        Log.d(TAG, "MainActivity onCreate");
        
        // Initialisation
        preferencesManager = new PreferencesManager(this);
        reconnectHandler = new Handler(Looper.getMainLooper());
        renderScript = RenderScript.create(this);
        
        // Initialiser les gestionnaires de tabs
        initializeTabManagers();
        
        // Setup UI
        initializeUI();
        
        // Vérification permissions et démarrage BLE
        checkPermissions();
        
        // Garder l'écran allumé si configuré
        if (preferencesManager.getKeepScreenOn()) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
        
        // Précharger tous les tabs en arrière-plan pour éviter la latence
        preloadAllTabs();
    }
    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "MainActivity onDestroy");
        
        cancelReconnectTimer();
        
        // Nettoyage RenderScript
        if (renderScript != null) {
            renderScript.destroy();
            renderScript = null;
        }
        
        try {
            if (serviceConnected) {
                if (bleService != null) {
                    bleService.setCallback(null);
                }
                unbindService(serviceConnection);
            }
        } catch (Exception e) {
            Log.e(TAG, "Erreur nettoyage: " + e.getMessage());
        }
    }
    
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        
        if (requestCode == REQUEST_PERMISSIONS) {
            boolean allGranted = true;
            for (int result : grantResults) {
                if (result != PackageManager.PERMISSION_GRANTED) {
                    allGranted = false;
                    break;
                }
            }
            
            if (allGranted) {
                initializeBluetooth();
            } else {
                Toast.makeText(this, "Permissions Bluetooth requises", Toast.LENGTH_LONG).show();
                finish();
            }
        }
    }
    
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        
        if (requestCode == REQUEST_ENABLE_BT) {
            if (resultCode == RESULT_OK) {
                startBleService();
            } else {
                Toast.makeText(this, "Bluetooth requis", Toast.LENGTH_LONG).show();
                finish();
            }
        }
    }
    
    // ==================== UI SETUP ====================
    
    private void initializeTabManagers() {
        Log.d(TAG, "Initialisation des gestionnaires de tabs");
        
        // Créer le command sender pour tous les tabs
        BaseTab.CommandSender commandSender = this::sendVanCommand;
        
        // Créer les instances des gestionnaires de tabs
        homeTab = new HomeTab();
        homeTab.setCommandSender(commandSender);
        
        lightTab = new LightTab(this, renderScript);
        lightTab.setCommandSender(commandSender);
        
        waterTab = new WaterTab();
        waterTab.setCommandSender(commandSender);
        
        heaterTab = new HeaterTab();
        heaterTab.setCommandSender(commandSender);
        
        energyTab = new EnergyTab();
        energyTab.setCommandSender(commandSender);
        
        multimediaTab = new MultimediaTab();
        multimediaTab.setCommandSender(commandSender);
        
        // Stocker dans le SparseArray pour accès rapide
        tabManagers.put(Tab.HOME.ordinal(), homeTab);
        tabManagers.put(Tab.LIGHT.ordinal(), lightTab);
        tabManagers.put(Tab.WATER.ordinal(), waterTab);
        tabManagers.put(Tab.HEATER.ordinal(), heaterTab);
        tabManagers.put(Tab.ENERGY.ordinal(), energyTab);
        tabManagers.put(Tab.MULTIMEDIA.ordinal(), multimediaTab);
    }
    
    /**
     * Envoie une commande VanCommand via le service BLE
     */
    private boolean sendVanCommand(VanCommand command) {
        if (bleService == null || !bleService.isConnected()) {
            Log.e(TAG, "Impossible d'envoyer la commande: pas de connexion BLE");
            Toast.makeText(this, "Pas de connexion BLE", Toast.LENGTH_SHORT).show();
            return false;
        }
        
        Log.d(TAG, "Envoi de commande: " + command.toString());
        return bleService.sendBinaryCommand(command);
    }
    
    private void initializeUI() {
        // Loading overlay
        loadingOverlay = findViewById(R.id.loading_overlay);
        
        // Bluetooth status UI
        bluetoothIconBg = findViewById(R.id.iv_bluetooth_bg);
        bluetoothIcon = findViewById(R.id.iv_bluetooth);
        bluetoothStatusText = findViewById(R.id.bluetooth_status_text);
        bluetoothServicesStatusText = findViewById(R.id.bluetooth_services_status_text);
        signalIcon = findViewById(R.id.iv_signal);
        bleLayout = findViewById(R.id.ble_layout);
        
        bluetoothStatusText.setText("Disconnected");
        bluetoothServicesStatusText.setText("Services Off");
        bluetoothIconBg.setColorFilter(ContextCompat.getColor(this, R.color.disconnected));
        signalIcon.setImageResource(R.drawable.ic_cell_off);
        
        // System Health status UI
        systemHealthBg = findViewById(R.id.system_status_bg);
        systemHealthLogo = findViewById(R.id.system_status_logo);
        systemHealthStatus = findViewById(R.id.system_health_status);
        systemHealthUptime = findViewById(R.id.system_health_uptime);
        systemLayout = findViewById(R.id.system_layout);
        
        systemHealthStatus.setText("System Health");
        systemHealthUptime.setText("No Data");
        systemHealthBg.setColorFilter(ContextCompat.getColor(this, R.color.disconnected));
        systemHealthLogo.setImageResource(R.drawable.ic_ko_logo);
        
        // Tab management
        contentContainer = findViewById(R.id.content_container);
        tabButtonHome = findViewById(R.id.tab_button_home);
        tabButtonLight = findViewById(R.id.tab_button_light);
        tabButtonWater = findViewById(R.id.tab_button_water);
        tabButtonHeater = findViewById(R.id.tab_button_heater);
        tabButtonEnergy = findViewById(R.id.tab_button_energy);
        tabButtonMultimedia = findViewById(R.id.tab_button_multimedia);
        
        switchToTab(Tab.HOME);
        
        tabButtonHome.setOnClickListener(v -> switchToTab(Tab.HOME));
        tabButtonLight.setOnClickListener(v -> switchToTab(Tab.LIGHT));
        tabButtonWater.setOnClickListener(v -> switchToTab(Tab.WATER));
        tabButtonHeater.setOnClickListener(v -> switchToTab(Tab.HEATER));
        tabButtonEnergy.setOnClickListener(v -> switchToTab(Tab.ENERGY));
        tabButtonMultimedia.setOnClickListener(v -> switchToTab(Tab.MULTIMEDIA));
        
        // Warning popup setup
        setupWarningPopup(); // TODO uncomment this for final app
        
        // Bluetooth popup setup
        setupBluetoothPopup();
        
        // System Health popup setup
        setupSystemHealthPopup();
    }
    
    private void setupWarningPopup() {
        // Inflate popup layout
        LayoutInflater inflater = LayoutInflater.from(this);
        warningPopup = inflater.inflate(R.layout.bluetooth_warning_popup, null);
        
        // Get references to popup views
        popupTitle = warningPopup.findViewById(R.id.title);
        statusMessage = warningPopup.findViewById(R.id.status_message);
        connectionStatusValue = warningPopup.findViewById(R.id.connection_status_value);
        reconnectAttemptsValue = warningPopup.findViewById(R.id.reconnect_attempts_value);
        actionValue = warningPopup.findViewById(R.id.action_value);
        ImageView bluetoothIconPopup = warningPopup.findViewById(R.id.bluetooth_icon_popup);
        
        // Start bluetooth icon animation
        android.view.animation.Animation pulse = android.view.animation.AnimationUtils.loadAnimation(this, R.anim.bluetooth_pulse);
        if (bluetoothIconPopup != null) {
            bluetoothIconPopup.startAnimation(pulse);
        }
        
        // Add popup to root layout (initially hidden)
        FrameLayout rootLayout = findViewById(android.R.id.content);
        rootLayout.addView(warningPopup);
        warningPopup.setVisibility(android.view.View.GONE);
        
        // Click on popup background to dismiss (optional)
        warningPopup.setOnClickListener(v -> {
            // Ne rien faire pour ne pas fermer accidentellement
        });
    }
    
    private void setupBluetoothPopup() {
        // Inflate popup layout
        LayoutInflater inflater = LayoutInflater.from(this);
        bluetoothPopup = inflater.inflate(R.layout.bluetooth_info_popup, null);
        
        // Get references to popup views
        popupConnectionStatus = bluetoothPopup.findViewById(R.id.popup_connection_status);
        popupServiceStatus = bluetoothPopup.findViewById(R.id.popup_service_status);
        popupDeviceName = bluetoothPopup.findViewById(R.id.popup_device_name);
        popupDeviceMac = bluetoothPopup.findViewById(R.id.popup_device_mac);
        popupDeviceRssi = bluetoothPopup.findViewById(R.id.popup_device_rssi);
        popupDeviceMtu = bluetoothPopup.findViewById(R.id.popup_device_mtu);
        popupServicesContainer = bluetoothPopup.findViewById(R.id.popup_services_container);
        popupConnectButton = bluetoothPopup.findViewById(R.id.bluetooth_action_button);
        popupConnectButtonText = bluetoothPopup.findViewById(R.id.bluetooth_action_button_text);
        
        // Add popup to root layout (initially hidden)
        FrameLayout rootLayout = findViewById(android.R.id.content);
        rootLayout.addView(bluetoothPopup);
        bluetoothPopup.setVisibility(android.view.View.GONE);
        
        // Click on close button to dismiss
        android.view.View closeButton = bluetoothPopup.findViewById(R.id.close_button);
        if (closeButton != null) {
            closeButton.setOnClickListener(v -> hideBluetoothPopup());
        }
        
        // Click on BLE layout to show popup
        if (bleLayout != null) {
            bleLayout.setOnClickListener(v -> {
                if (serviceConnected && bleService != null) {
                    showBluetoothPopup();
                }
            });
        }
        
        // Setup connect button
        popupConnectButton.setOnClickListener(v -> {
            if (!serviceConnected || bleService == null) {
                Toast.makeText(this, "Service BLE non prêt", Toast.LENGTH_SHORT).show();
                return;
            }
            
            if (bleService.isConnected()) {
                bleService.disconnect();
            } else if (bleService.isScanning()) {
                bleService.stopScan();
            } else {
                bleService.startScan();
            }
            
            hideBluetoothPopup();
            updateConnectionStatus();
        });
    }
    
    private void showBluetoothPopup() {
        if (bluetoothPopup == null || bleService == null) return;
        
        // Update popup content
        if (bleService.isConnected()) {
            popupConnectionStatus.setText("Connected");
            popupConnectionStatus.setTextColor(ContextCompat.getColor(this, R.color.connected));
        } else {
            popupConnectionStatus.setText("Disconnected");
            popupConnectionStatus.setTextColor(ContextCompat.getColor(this, R.color.disconnected));
        }
        
        if (bleService.areServicesDiscovered()) {
            popupServiceStatus.setText("Services Ready");
            popupServiceStatus.setTextColor(ContextCompat.getColor(this, R.color.connected));
        } else {
            popupServiceStatus.setText("Not Ready");
            popupServiceStatus.setTextColor(ContextCompat.getColor(this, R.color.warning));
        }
        
        popupDeviceName.setText(bleService.getDeviceName());
        popupDeviceMac.setText(bleService.getDeviceMacAddress());
        popupDeviceRssi.setText(bleService.getDeviceRssi() + " dBm");
        popupDeviceMtu.setText(bleService.getCurrentMtu() + " bytes");
        
        // Update services list
        popupServicesContainer.removeAllViews();
        java.util.List<String> services = bleService.getDiscoveredServices();
        if (services.isEmpty()) {
            TextView noServices = new TextView(this);
            noServices.setText("No services discovered");
            noServices.setTextColor(ContextCompat.getColor(this, R.color.text_tertiary));
            noServices.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, 12);
            popupServicesContainer.addView(noServices);
        } else {
            for (String serviceUuid : services) {
                TextView serviceView = new TextView(this);
                serviceView.setText(serviceUuid);
                serviceView.setTextColor(ContextCompat.getColor(this, R.color.white));
                serviceView.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, 11);
                serviceView.setTypeface(android.graphics.Typeface.MONOSPACE);
                LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                );
                params.bottomMargin = 8;
                serviceView.setLayoutParams(params);
                popupServicesContainer.addView(serviceView);
            }
        }
        
        // Update button text
        if (bleService.isConnected()) {
            popupConnectButtonText.setText("🔌 Disconnect");
        } else if (bleService.isScanning()) {
            popupConnectButtonText.setText("⏹️ Cancel");
        } else {
            popupConnectButtonText.setText("🔌 Connect");
        }
        
        // Show popup
        bluetoothPopup.setVisibility(android.view.View.VISIBLE);
    }
    
    private void hideBluetoothPopup() {
        if (bluetoothPopup != null) {
            bluetoothPopup.setVisibility(android.view.View.GONE);
        }
    }
    
    // ==================== SYSTEM HEALTH POPUP ====================
    
    private void setupSystemHealthPopup() {
        // Inflate popup layout
        LayoutInflater inflater = LayoutInflater.from(this);
        systemHealthPopup = inflater.inflate(R.layout.system_health_popup, null);
        
        // Get references to popup containers
        overviewContainer = systemHealthPopup.findViewById(R.id.overview_container);
        mainErrorsContainer = systemHealthPopup.findViewById(R.id.main_errors_container);
        slaveErrorsContainer = systemHealthPopup.findViewById(R.id.slave_errors_container);
        vanStateContainer = systemHealthPopup.findViewById(R.id.van_state_container);
        
        // Add popup to root layout (initially hidden)
        FrameLayout rootLayout = findViewById(android.R.id.content);
        rootLayout.addView(systemHealthPopup);
        systemHealthPopup.setVisibility(android.view.View.GONE);
        
        // Click on close button to dismiss
        android.view.View closeButton = systemHealthPopup.findViewById(R.id.close_button);
        if (closeButton != null) {
            closeButton.setOnClickListener(v -> hideSystemHealthPopup());
        }
        
        // Click on system layout to show popup
        if (systemLayout != null) {
            systemLayout.setOnClickListener(v -> {
                if (lastVanState != null) {
                    showSystemHealthPopup();
                } else {
                    Toast.makeText(this, "No van state data available", Toast.LENGTH_SHORT).show();
                }
            });
        }
    }
    
    private void showSystemHealthPopup() {
        if (systemHealthPopup == null || lastVanState == null) {
            android.util.Log.d("SystemHealth", "Cannot show popup: systemHealthPopup=" + (systemHealthPopup != null) + ", lastVanState=" + (lastVanState != null));
            return;
        }
        
        android.util.Log.d("SystemHealth", "Showing popup with lastVanState");
        
        // Clear all containers
        overviewContainer.removeAllViews();
        mainErrorsContainer.removeAllViews();
        slaveErrorsContainer.removeAllViews();
        vanStateContainer.removeAllViews();
        
        // Populate overview
        addInfoRow(overviewContainer, "System Status:", getSystemStatusText(), getSystemStatusColor());
        if (lastVanState.system != null) {
            addInfoRow(overviewContainer, "Uptime:", formatUptime(lastVanState.system.uptime), ContextCompat.getColor(this, R.color.white));
        }
        addInfoRow(overviewContainer, "Total Errors:", String.valueOf(getTotalErrorCount()), getTotalErrorCount() > 0 ? ContextCompat.getColor(this, R.color.error) : ContextCompat.getColor(this, R.color.white));
        addInfoRow(overviewContainer, "BLE Connected:", bleService != null && bleService.isConnected() ? "Yes" : "No", bleService != null && bleService.isConnected() ? ContextCompat.getColor(this, R.color.connected) : ContextCompat.getColor(this, R.color.disconnected));
        
        // Populate main system errors
        if (lastVanState.system != null && lastVanState.system.system_error) {
            addInfoRow(mainErrorsContainer, "System Error:", "Yes", ContextCompat.getColor(this, R.color.error));
            addInfoRow(mainErrorsContainer, "Error Code:", String.valueOf(lastVanState.system.error_code), ContextCompat.getColor(this, R.color.error));
        } else {
            addInfoRow(mainErrorsContainer, "System Error:", "No (0 errors)", ContextCompat.getColor(this, R.color.connected));
        }
        
        // Populate slave PCB errors
        if (lastVanState.slave_pcb != null && lastVanState.slave_pcb.error_state != null) {
            VanState.SlaveErrorStats stats = lastVanState.slave_pcb.error_state.error_stats;
            if (stats != null) {
                int totalErrors = (int) stats.total_errors;
                addInfoRow(slaveErrorsContainer, "Total Errors:", String.valueOf(totalErrors) + (totalErrors == 0 ? "" : " detected"), totalErrors > 0 ? ContextCompat.getColor(this, R.color.error) : ContextCompat.getColor(this, R.color.connected));
                
                if (totalErrors > 0) {
                    addInfoRow(slaveErrorsContainer, "Low Severity:", String.valueOf(stats.errors_by_severity[0]), ContextCompat.getColor(this, R.color.white));
                    addInfoRow(slaveErrorsContainer, "Medium Severity:", String.valueOf(stats.errors_by_severity[1]), ContextCompat.getColor(this, R.color.warning));
                    addInfoRow(slaveErrorsContainer, "High Severity:", String.valueOf(stats.errors_by_severity[2]), ContextCompat.getColor(this, R.color.error));
                    addInfoRow(slaveErrorsContainer, "Critical Severity:", String.valueOf(stats.errors_by_severity[3]), ContextCompat.getColor(this, R.color.error));
                    
                    // Show last errors
                    for (int i = 0; i < lastVanState.slave_pcb.error_state.last_errors.length; i++) {
                        VanState.SlaveErrorEvent error = lastVanState.slave_pcb.error_state.last_errors[i];
                        if (error != null && error.timestamp > 0) {
                            String errorText = String.format("Error #%d: %s (%s)", i + 1, error.description != null ? error.description : "Unknown", error.module != null ? error.module : "N/A");
                            addInfoRow(slaveErrorsContainer, errorText, "", ContextCompat.getColor(this, R.color.white));
                        }
                    }
                }
            }
        } else {
            addInfoRow(slaveErrorsContainer, "Total Errors:", "0", ContextCompat.getColor(this, R.color.connected));
        }
        
        // Populate complete van state
        populateVanStateDetails();
        
        // Show popup
        systemHealthPopup.setVisibility(android.view.View.VISIBLE);
    }
    
    private void hideSystemHealthPopup() {
        if (systemHealthPopup != null) {
            systemHealthPopup.setVisibility(android.view.View.GONE);
        }
    }
    
    private void addInfoRow(LinearLayout container, String label, String value, int valueColor) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(android.view.Gravity.CENTER_VERTICAL);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        params.bottomMargin = 24;
        row.setLayoutParams(params);
        
        TextView labelView = new TextView(this);
        labelView.setText(label);
        labelView.setTextColor(ContextCompat.getColor(this, R.color.white));
        labelView.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, 16);
        LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(
            0,
            LinearLayout.LayoutParams.WRAP_CONTENT,
            0.45f
        );
        labelParams.setMarginEnd(32);
        labelView.setLayoutParams(labelParams);
        labelView.setGravity(android.view.Gravity.END);
        
        TextView valueView = new TextView(this);
        valueView.setText(value);
        valueView.setTextColor(valueColor);
        valueView.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, 16);
        valueView.setTypeface(null, android.graphics.Typeface.BOLD);
        LinearLayout.LayoutParams valueParams = new LinearLayout.LayoutParams(
            0,
            LinearLayout.LayoutParams.WRAP_CONTENT,
            0.55f
        );
        valueParams.setMarginStart(32);
        valueView.setLayoutParams(valueParams);
        
        row.addView(labelView);
        row.addView(valueView);
        container.addView(row);
    }
    
    private void populateVanStateDetails() {
        android.util.Log.d("SystemHealth", "populateVanStateDetails() called");
        
        // Energy - MPPT 100/50
        addSectionTitle(vanStateContainer, "⚡ Energy - MPPT 100/50");
        if (lastVanState.mppt != null) {
            android.util.Log.d("SystemHealth", "Adding MPPT 100/50 data");
            addInfoRow(vanStateContainer, "Solar Power:", String.format("%.1fW", lastVanState.mppt.solar_power_100_50), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Panel Voltage:", String.format("%.2fV", lastVanState.mppt.panel_voltage_100_50), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Panel Current:", String.format("%.2fA", lastVanState.mppt.panel_current_100_50), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Battery Voltage:", String.format("%.2fV", lastVanState.mppt.battery_voltage_100_50), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Battery Current:", String.format("%.2fA", lastVanState.mppt.battery_current_100_50), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Temperature:", String.format("%d°C", lastVanState.mppt.temperature_100_50), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "State:", String.valueOf(lastVanState.mppt.state_100_50), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Error Flags:", String.valueOf(lastVanState.mppt.error_flags_100_50), lastVanState.mppt.error_flags_100_50 != 0 ? ContextCompat.getColor(this, R.color.error) : ContextCompat.getColor(this, R.color.white));
        }
        
        // Energy - MPPT 70/15
        addSectionTitle(vanStateContainer, "⚡ Energy - MPPT 70/15");
        if (lastVanState.mppt != null) {
            addInfoRow(vanStateContainer, "Solar Power:", String.format("%.1fW", lastVanState.mppt.solar_power_70_15), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Panel Voltage:", String.format("%.2fV", lastVanState.mppt.panel_voltage_70_15), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Panel Current:", String.format("%.2fA", lastVanState.mppt.panel_current_70_15), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Battery Voltage:", String.format("%.2fV", lastVanState.mppt.battery_voltage_70_15), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Battery Current:", String.format("%.2fA", lastVanState.mppt.battery_current_70_15), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Temperature:", String.format("%d°C", lastVanState.mppt.temperature_70_15), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "State:", String.valueOf(lastVanState.mppt.state_70_15), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Error Flags:", String.valueOf(lastVanState.mppt.error_flags_70_15), lastVanState.mppt.error_flags_70_15 != 0 ? ContextCompat.getColor(this, R.color.error) : ContextCompat.getColor(this, R.color.white));
        }
        
        // Energy - Battery
        addSectionTitle(vanStateContainer, "🔋 Energy - Battery");
        if (lastVanState.battery != null) {
            addInfoRow(vanStateContainer, "Voltage:", String.format("%d mV (%.2fV)", lastVanState.battery.voltage_mv, lastVanState.battery.voltage_mv / 1000.0f), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Current:", String.format("%d mA (%.2fA)", lastVanState.battery.current_ma, lastVanState.battery.current_ma / 1000.0f), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Capacity:", String.format("%d mAh", lastVanState.battery.capacity_mah), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "SOC:", String.format("%d%%", lastVanState.battery.soc_percent), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Cell Count:", String.valueOf(lastVanState.battery.cell_count), ContextCompat.getColor(this, R.color.white));
            
            // Cell voltages
            for (int i = 0; i < lastVanState.battery.cell_count && i < lastVanState.battery.cell_voltage_mv.length; i++) {
                addInfoRow(vanStateContainer, String.format("Cell %d Voltage:", i + 1), String.format("%d mV", lastVanState.battery.cell_voltage_mv[i]), ContextCompat.getColor(this, R.color.white));
            }
            
            addInfoRow(vanStateContainer, "Temp Sensor Count:", String.valueOf(lastVanState.battery.temp_sensor_count), ContextCompat.getColor(this, R.color.white));
            
            // Temperatures
            for (int i = 0; i < lastVanState.battery.temp_sensor_count && i < lastVanState.battery.temperatures_c.length; i++) {
                addInfoRow(vanStateContainer, String.format("Temperature %d:", i + 1), String.format("%d°C", lastVanState.battery.temperatures_c[i]), ContextCompat.getColor(this, R.color.white));
            }
            
            addInfoRow(vanStateContainer, "Cycle Count:", String.valueOf(lastVanState.battery.cycle_count), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Nominal Capacity:", String.format("%d mAh", lastVanState.battery.nominal_capacity_mah), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Design Capacity:", String.format("%d mAh", lastVanState.battery.design_capacity_mah), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Health:", String.format("%d%%", lastVanState.battery.health_percent), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "MOSFET Status:", String.valueOf(lastVanState.battery.mosfet_status), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Protection Status:", String.valueOf(lastVanState.battery.protection_status), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Balance Status:", String.valueOf(lastVanState.battery.balance_status), ContextCompat.getColor(this, R.color.white));
        }
        
        // Sensors
        addSectionTitle(vanStateContainer, "🌡️ Sensors");
        if (lastVanState.sensors != null) {
            addInfoRow(vanStateContainer, "Cabin Temp:", String.format("%.1f°C", lastVanState.sensors.cabin_temperature), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Exterior Temp:", String.format("%.1f°C", lastVanState.sensors.exterior_temperature), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Humidity:", String.format("%.1f%%", lastVanState.sensors.humidity), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "CO2:", String.format("%d ppm", lastVanState.sensors.co2_level), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Light Level:", String.format("%d raw adc", lastVanState.sensors.light), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Door:", lastVanState.sensors.door_open ? "Open" : "Closed", lastVanState.sensors.door_open ? ContextCompat.getColor(this, R.color.warning) : ContextCompat.getColor(this, R.color.white));
        }
        
        // Heater
        addSectionTitle(vanStateContainer, "🔥 Heater");
        if (lastVanState.heater != null) {
            addInfoRow(vanStateContainer, "Status:", lastVanState.heater.heater_on ? "ON" : "OFF", lastVanState.heater.heater_on ? ContextCompat.getColor(this, R.color.connected) : ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Target Air Temp:", String.format("%.1f°C", lastVanState.heater.target_air_temperature), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Actual Air Temp:", String.format("%.1f°C", lastVanState.heater.actual_air_temperature), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Antifreeze Temp:", String.format("%.1f°C", lastVanState.heater.antifreeze_temperature), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Fuel Level:", String.format("%d%%", lastVanState.heater.fuel_level_percent), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Error Code:", String.valueOf(lastVanState.heater.error_code), lastVanState.heater.error_code != 0 ? ContextCompat.getColor(this, R.color.error) : ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Pump Active:", lastVanState.heater.pump_active ? "Yes" : "No", ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Radiator Fan Speed:", String.format("%d%%", lastVanState.heater.radiator_fan_speed), ContextCompat.getColor(this, R.color.white));
        }
        
        // LEDs - Roof 1
        addSectionTitle(vanStateContainer, "💡 Lighting - Roof 1");
        if (lastVanState.leds != null && lastVanState.leds.leds_roof1 != null) {
            addInfoRow(vanStateContainer, "Enabled:", lastVanState.leds.leds_roof1.enabled ? "ON" : "OFF", lastVanState.leds.leds_roof1.enabled ? ContextCompat.getColor(this, R.color.connected) : ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Mode:", String.valueOf(lastVanState.leds.leds_roof1.current_mode), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Brightness:", String.format("%d%%", lastVanState.leds.leds_roof1.brightness), ContextCompat.getColor(this, R.color.white));
        }
        
        // LEDs - Roof 2
        addSectionTitle(vanStateContainer, "💡 Lighting - Roof 2");
        if (lastVanState.leds != null && lastVanState.leds.leds_roof2 != null) {
            addInfoRow(vanStateContainer, "Enabled:", lastVanState.leds.leds_roof2.enabled ? "ON" : "OFF", lastVanState.leds.leds_roof2.enabled ? ContextCompat.getColor(this, R.color.connected) : ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Mode:", String.valueOf(lastVanState.leds.leds_roof2.current_mode), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Brightness:", String.format("%d%%", lastVanState.leds.leds_roof2.brightness), ContextCompat.getColor(this, R.color.white));
        }
        
        // LEDs - Front
        addSectionTitle(vanStateContainer, "💡 Lighting - Front");
        if (lastVanState.leds != null && lastVanState.leds.leds_av != null) {
            addInfoRow(vanStateContainer, "Enabled:", lastVanState.leds.leds_av.enabled ? "ON" : "OFF", lastVanState.leds.leds_av.enabled ? ContextCompat.getColor(this, R.color.connected) : ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Mode:", String.valueOf(lastVanState.leds.leds_av.current_mode), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Brightness:", String.format("%d%%", lastVanState.leds.leds_av.brightness), ContextCompat.getColor(this, R.color.white));
        }
        
        // LEDs - Rear
        addSectionTitle(vanStateContainer, "💡 Lighting - Rear");
        if (lastVanState.leds != null && lastVanState.leds.leds_ar != null) {
            addInfoRow(vanStateContainer, "Enabled:", lastVanState.leds.leds_ar.enabled ? "ON" : "OFF", lastVanState.leds.leds_ar.enabled ? ContextCompat.getColor(this, R.color.connected) : ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Mode:", String.valueOf(lastVanState.leds.leds_ar.current_mode), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Brightness:", String.format("%d%%", lastVanState.leds.leds_ar.brightness), ContextCompat.getColor(this, R.color.white));
        }
        
        // System
        addSectionTitle(vanStateContainer, "⚙️ System");
        if (lastVanState.system != null) {
            addInfoRow(vanStateContainer, "Uptime:", formatUptime(lastVanState.system.uptime), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "System Error:", lastVanState.system.system_error ? "Yes" : "No", lastVanState.system.system_error ? ContextCompat.getColor(this, R.color.error) : ContextCompat.getColor(this, R.color.connected));
            addInfoRow(vanStateContainer, "Error Code:", String.valueOf(lastVanState.system.error_code), lastVanState.system.error_code != 0 ? ContextCompat.getColor(this, R.color.error) : ContextCompat.getColor(this, R.color.white));
        }
        
        // Slave PCB
        addSectionTitle(vanStateContainer, "🔌 Slave PCB");
        if (lastVanState.slave_pcb != null) {
            addInfoRow(vanStateContainer, "Timestamp:", String.valueOf(lastVanState.slave_pcb.timestamp), ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Current Case:", lastVanState.slave_pcb.current_case != null ? lastVanState.slave_pcb.current_case.name() : "N/A", ContextCompat.getColor(this, R.color.white));
            addInfoRow(vanStateContainer, "Hood State:", lastVanState.slave_pcb.hood_state != null ? lastVanState.slave_pcb.hood_state.name() : "N/A", ContextCompat.getColor(this, R.color.white));
            
            if (lastVanState.slave_pcb.system_health != null) {
                addInfoRow(vanStateContainer, "System Healthy:", lastVanState.slave_pcb.system_health.system_healthy ? "Yes" : "No", lastVanState.slave_pcb.system_health.system_healthy ? ContextCompat.getColor(this, R.color.connected) : ContextCompat.getColor(this, R.color.error));
                addInfoRow(vanStateContainer, "Last Health Check:", String.valueOf(lastVanState.slave_pcb.system_health.last_health_check), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Uptime:", String.format("%d seconds", lastVanState.slave_pcb.system_health.uptime_seconds), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Free Heap:", String.format("%d bytes", lastVanState.slave_pcb.system_health.free_heap_size), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Min Free Heap:", String.format("%d bytes", lastVanState.slave_pcb.system_health.min_free_heap_size), ContextCompat.getColor(this, R.color.white));
            }
        }
        
        // Water Tanks
        addSectionTitle(vanStateContainer, "💧 Water Tanks");
        if (lastVanState.slave_pcb != null && lastVanState.slave_pcb.tanks_levels != null) {
            VanState.WaterTanksLevels tanks = lastVanState.slave_pcb.tanks_levels;
            
            if (tanks.tank_a != null) {
                addInfoRow(vanStateContainer, "Tank A - Level:", String.format("%.1f%%", tanks.tank_a.level_percentage), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Tank A - Weight:", String.format("%.2f kg", tanks.tank_a.weight_kg), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Tank A - Volume:", String.format("%.1f L", tanks.tank_a.volume_liters), ContextCompat.getColor(this, R.color.white));
            }
            
            if (tanks.tank_b != null) {
                addInfoRow(vanStateContainer, "Tank B - Level:", String.format("%.1f%%", tanks.tank_b.level_percentage), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Tank B - Weight:", String.format("%.2f kg", tanks.tank_b.weight_kg), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Tank B - Volume:", String.format("%.1f L", tanks.tank_b.volume_liters), ContextCompat.getColor(this, R.color.white));
            }
            
            if (tanks.tank_c != null) {
                addInfoRow(vanStateContainer, "Tank C - Level:", String.format("%.1f%%", tanks.tank_c.level_percentage), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Tank C - Weight:", String.format("%.2f kg", tanks.tank_c.weight_kg), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Tank C - Volume:", String.format("%.1f L", tanks.tank_c.volume_liters), ContextCompat.getColor(this, R.color.white));
            }
            
            if (tanks.tank_d != null) {
                addInfoRow(vanStateContainer, "Tank D - Level:", String.format("%.1f%%", tanks.tank_d.level_percentage), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Tank D - Weight:", String.format("%.2f kg", tanks.tank_d.weight_kg), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Tank D - Volume:", String.format("%.1f L", tanks.tank_d.volume_liters), ContextCompat.getColor(this, R.color.white));
            }
            
            if (tanks.tank_e != null) {
                addInfoRow(vanStateContainer, "Tank E - Level:", String.format("%.1f%%", tanks.tank_e.level_percentage), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Tank E - Weight:", String.format("%.2f kg", tanks.tank_e.weight_kg), ContextCompat.getColor(this, R.color.white));
                addInfoRow(vanStateContainer, "Tank E - Volume:", String.format("%.1f L", tanks.tank_e.volume_liters), ContextCompat.getColor(this, R.color.white));
            }
        }
    }
    
    private void addSectionTitle(LinearLayout container, String title) {
        TextView titleView = new TextView(this);
        titleView.setText(title);
        titleView.setTextColor(ContextCompat.getColor(this, R.color.white));
        titleView.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, 18);
        titleView.setTypeface(null, android.graphics.Typeface.BOLD);
        titleView.setGravity(android.view.Gravity.CENTER);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        params.topMargin = 48;
        params.bottomMargin = 32;
        titleView.setLayoutParams(params);
        container.addView(titleView);
        
        // Add separator
        View separator = new View(this);
        separator.setBackgroundResource(R.drawable.spacer_line);
        LinearLayout.LayoutParams sepParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            4
        );
        sepParams.bottomMargin = 32;
        separator.setLayoutParams(sepParams);
        container.addView(separator);
    }
    
    private int getTotalErrorCount() {
        int count = 0;
        if (lastVanState == null) return count;
        
        // Main system errors
        if (lastVanState.system != null && lastVanState.system.system_error) {
            count++;
        }
        
        // Slave PCB errors
        if (lastVanState.slave_pcb != null && lastVanState.slave_pcb.error_state != null && 
            lastVanState.slave_pcb.error_state.error_stats != null) {
            count += (int) lastVanState.slave_pcb.error_state.error_stats.total_errors;
        }
        
        return count;
    }
    
    private String getSystemStatusText() {
        int errorCount = getTotalErrorCount();
        if (errorCount == 0) return "Healthy";
        if (errorCount <= 1) return "Warning";
        return "Critical";
    }
    
    private int getSystemStatusColor() {
        int errorCount = getTotalErrorCount();
        if (errorCount == 0) return ContextCompat.getColor(this, R.color.connected);
        if (errorCount <= 1) return ContextCompat.getColor(this, R.color.warning);
        return ContextCompat.getColor(this, R.color.error);
    }
    
    private String formatUptime(long uptimeSeconds) {
        if (uptimeSeconds == 0) return "No Data";
        
        long days = uptimeSeconds / 86400;
        long hours = (uptimeSeconds % 86400) / 3600;
        long minutes = (uptimeSeconds % 3600) / 60;
        long seconds = uptimeSeconds % 60;
        
        return String.format("%02dj:%02dh:%02dm:%02ds", days, hours, minutes, seconds);
    }
    
    /**
     * Met à jour l'affichage du System Health
     */
    private void updateSystemHealth(VanState vanState) {
        if (vanState == null || vanState.system == null) return;
        
        // Update uptime
        systemHealthUptime.setText(formatUptime(vanState.system.uptime));
        
        // Calculate total errors
        int errorCount = getTotalErrorCount();
        
        // Update status based on error count
        if (errorCount == 0) {
            // Healthy
            systemHealthStatus.setText("System Healthy");
            systemHealthBg.setColorFilter(ContextCompat.getColor(this, R.color.connected));
            systemHealthLogo.setImageResource(R.drawable.ic_ok_logo);
        } else if (errorCount <= 5) {
            // Warning
            systemHealthStatus.setText("System Warning");
            systemHealthBg.setColorFilter(ContextCompat.getColor(this, R.color.warning));
            systemHealthLogo.setImageResource(R.drawable.ic_ko_logo);
        } else {
            // Error
            systemHealthStatus.setText("System Error");
            systemHealthBg.setColorFilter(ContextCompat.getColor(this, R.color.error));
            systemHealthLogo.setImageResource(R.drawable.ic_ko_logo);
        }
    }

    // ==================== TAB MANAGEMENT ====================
    
    private void switchToTab(Tab tab) {
        if (currentTab == tab) return;
        
        currentTab = tab;
        
        // Update button states
        resetButtonColors();
        ImageButton selectedButton = getButtonForTab(tab);
        if (selectedButton != null) {
            selectedButton.setSelected(true);
            selectedButton.setColorFilter(ContextCompat.getColor(this, R.color.button_selected));
        }
        
        // Load tab content
        loadTabContent(tab);
    }
    
    private void loadTabContent(Tab tab) {
        long startTime = System.currentTimeMillis();
        
        // Cacher toutes les vues déjà en cache
        for (int i = 0; i < cachedTabViews.size(); i++) {
            View cachedView = cachedTabViews.valueAt(i);
            if (cachedView != null && cachedView.getParent() == contentContainer) {
                cachedView.setVisibility(View.GONE);
            }
        }
        
        // Vérifier si la vue est déjà en cache
        View contentView = cachedTabViews.get(tab.ordinal());
        boolean isFirstLoad = (contentView == null);
        
        if (contentView == null) {
            // Première fois : inflater le layout (lent)
            Log.d(TAG, "Inflation du layout pour tab: " + tab.name());
            LayoutInflater inflater = LayoutInflater.from(this);
            contentView = inflater.inflate(tabLayouts[tab.ordinal()], contentContainer, false);
            
            // Ajouter au container
            contentContainer.addView(contentView);
            
            // Mettre en cache pour la prochaine fois
            cachedTabViews.put(tab.ordinal(), contentView);
        } else {
            // Vue déjà créée : réutilisation (ultra rapide)
            Log.d(TAG, "Réutilisation du cache pour tab: " + tab.name());
            
            // Vérifier si elle est déjà attachée au bon parent
            if (contentView.getParent() != contentContainer) {
                // Si elle a un autre parent, la détacher d'abord
                if (contentView.getParent() != null) {
                    ((android.view.ViewGroup) contentView.getParent()).removeView(contentView);
                }
                contentContainer.addView(contentView);
            }
        }
        
        // Rendre visible
        contentView.setVisibility(View.VISIBLE);
        
        // Récupérer le gestionnaire du tab
        BaseTab tabManager = tabManagers.get(tab.ordinal());
        
        // Initialiser le tab manager s'il ne l'est pas encore
        if (tabManager != null && !tabManager.isInitialized()) {
            Log.d(TAG, "Initialisation du tab manager pour: " + tab.name());
            tabManager.initialize(contentView);
        }
        
        // Mettre à jour l'UI avec les dernières données
        if (tabManager != null && lastVanState != null) {
            tabManager.updateUI(lastVanState);
        }
        
        // Notifier le tab qu'il est sélectionné
        if (tabManager != null) {
            tabManager.onTabSelected();
        }
        
        long endTime = System.currentTimeMillis();
        Log.d(TAG, "Chargement tab " + tab.name() + " en " + (endTime - startTime) + "ms");
    }
    
    private void reloadCurrentTab() {
        if (currentTab != Tab.NONE) {
            Log.d(TAG, "Rechargement du layout actuel: " + currentTab.name());
            
            // Réinitialiser les données
            lastVanState = null;
            
            // Supprimer du cache pour forcer un rechargement
            cachedTabViews.remove(currentTab.ordinal());
            
            // Recharger le layout pour avoir les valeurs par défaut du XML
            loadTabContent(currentTab);
            
        }
    }
    
    private void resetButtonColors() {
        int whiteColor = ContextCompat.getColor(this, android.R.color.white);
        tabButtonHome.setSelected(false);
        tabButtonHome.setColorFilter(whiteColor);
        tabButtonLight.setSelected(false);
        tabButtonLight.setColorFilter(whiteColor);
        tabButtonWater.setSelected(false);
        tabButtonWater.setColorFilter(whiteColor);
        tabButtonHeater.setSelected(false);
        tabButtonHeater.setColorFilter(whiteColor);
        tabButtonEnergy.setSelected(false);
        tabButtonEnergy.setColorFilter(whiteColor);
        tabButtonMultimedia.setSelected(false);
        tabButtonMultimedia.setColorFilter(whiteColor);
    }
    
    private ImageButton getButtonForTab(Tab tab) {
        switch (tab) {
            case HOME: return tabButtonHome;
            case LIGHT: return tabButtonLight;
            case WATER: return tabButtonWater;
            case HEATER: return tabButtonHeater;
            case ENERGY: return tabButtonEnergy;
            case MULTIMEDIA: return tabButtonMultimedia;
        }
        return null;
    }
    
    // ==================== UI UPDATE ====================
    
    private void updateUI(VanState vanState) {
        if (vanState == null) {
            Log.w(TAG, "updateUI: vanState null");
            return;
        }

        // Sauvegarder l'état
        lastVanState = vanState;
        
        // Mettre à jour le System Health
        updateSystemHealth(vanState);
        
        // Mettre à jour la popup si elle est visible
        if (systemHealthPopup != null && systemHealthPopup.getVisibility() == android.view.View.VISIBLE) {
            showSystemHealthPopup();
        }
        
        // Mettre à jour le tab actuel
        if (currentTab != Tab.NONE) {
            BaseTab tabManager = tabManagers.get(currentTab.ordinal());
            if (tabManager != null) {
                Log.d(TAG, "Mise à jour du tab: " + currentTab.name());
                tabManager.updateUI(vanState);
            } else {
                Log.w(TAG, "Aucun tab manager trouvé pour: " + currentTab.name());
            }
        }
    }
    
    
    
    // ==================== BLUETOOTH SETUP ====================
    
    private void checkPermissions() {
        String[] permissions = {
            Manifest.permission.BLUETOOTH,
            Manifest.permission.BLUETOOTH_ADMIN,
            Manifest.permission.BLUETOOTH_CONNECT,
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.ACCESS_FINE_LOCATION
        };
        
        boolean allGranted = true;
        for (String permission : permissions) {
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
                allGranted = false;
                break;
            }
        }
        
        if (!allGranted) {
            ActivityCompat.requestPermissions(this, permissions, REQUEST_PERMISSIONS);
        } else {
            initializeBluetooth();
        }
    }
    
    private void initializeBluetooth() {
        BluetoothManager bluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter bluetoothAdapter = bluetoothManager.getAdapter();
        
        if (bluetoothAdapter == null) {
            Toast.makeText(this, "Bluetooth non supporté", Toast.LENGTH_LONG).show();
            return;
        }
        
        if (!bluetoothAdapter.isEnabled()) {
            Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT);
        } else {
            startBleService();
        }
    }
    
    private void startBleService() {
        Log.d(TAG, "Démarrage service BLE");
        Intent intent = new Intent(this, VanBleService.class);
        bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
    }
    // ==================== BLE SERVICE CONNECTION ====================
    
    private final ServiceConnection serviceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            VanBleService.LocalBinder binder = (VanBleService.LocalBinder) service;
            bleService = binder.getService();
            serviceConnected = true;
            
            // Configuration du callback direct (pas de broadcast receiver nécessaire)
            bleService.setCallback(MainActivity.this);
            
            updateConnectionStatus();
            Log.d(TAG, "Service BLE connecté");
            
            // Auto-connexion si configuré
            if (preferencesManager.getAutoConnect() && !bleService.isConnected()) {
                bleService.startScan();
            }
        }
        
        @Override
        public void onServiceDisconnected(ComponentName name) {
            serviceConnected = false;
            bleService = null;
            Log.d(TAG, "Service BLE déconnecté");
        }
    };
    
    // ==================== BLE CALLBACKS ====================
    
    // Variable pour éviter les callbacks multiples
    private boolean isConnected = false;
    
    @Override
    public void onConnectionStateChanged(boolean connected) {
        // Ignorer si c'est le même état (évite les callbacks en boucle)
        if (isConnected == connected) {
            Log.d(TAG, "Callback connexion ignoré (même état): " + connected);
            return;
        }
        
        isConnected = connected;
        Log.d(TAG, "Callback connexion: " + connected);
        
        runOnUiThread(() -> {
            updateConnectionStatus();
            
            if (connected) {
                // Première connexion réussie
                hasBeenConnectedBefore = true;
                Toast.makeText(this, "Connecté au van", Toast.LENGTH_SHORT).show();
                hideWarningPopup();
            } else {
                // Déconnexion - NE PAS reloader le tab, juste réinitialiser les données
                lastVanState = null;
                
                if (hasBeenConnectedBefore) {
                    // C'était une vraie déconnexion (perte de connexion)
                    Toast.makeText(this, "Déconnecté", Toast.LENGTH_SHORT).show();
                    showWarningPopup("Connection lost");
                } else {
                    // Première tentative de connexion (jamais connecté)
                    showWarningPopup("Connecting...");
                }
            }
        });
        
        if (connected) {
            resetReconnectAttempts();
        } else {
            scheduleReconnect();
        }
    }
    
    @Override
    public void onDataReceived(String jsonData) {
        // Si on reçoit des données, c'est qu'on est connecté
        try {
            if (lastVanState == null) {
                lastVanState = new VanState();
                Log.d(TAG, "Nouveau VanState créé");
            }
            VanStateParser.parseVanState(lastVanState, jsonData);
            Log.d(TAG, "Données parsées, mise à jour UI sur tab: " + currentTab.name());
            
            runOnUiThread(() -> updateUI(lastVanState));
        } catch (Exception e) {
            Log.e(TAG, "Erreur parsing JSON: " + e.getMessage());
        }
    }
    
    @Override
    public void onServicesDiscovered() {
        Log.d(TAG, "Services découverts");
        runOnUiThread(() -> {
            // Mettre à jour le statut complet (gère les services + couleur)
            updateConnectionStatus();
        });
    }
    
    @Override
    public void onRssiUpdated(int rssi) {
        Log.d(TAG, "RSSI mis à jour: " + rssi + " dBm");
        runOnUiThread(() -> {
            updateSignalIcon(rssi);
            updatePopupRssi(rssi);
        });
    }
    
    private void updateSignalIcon(int rssi) {
        // Convertir RSSI en niveau de signal (0-5 barres)
        int level;
        if (rssi >= -50) {
            level = 5;
        } else if (rssi >= -60) {
            level = 4;
        } else if (rssi >= -70) {
            level = 3;
        } else if (rssi >= -80) {
            level = 2;
        } else if (rssi >= -90) {
            level = 1;
        } else {
            level = 0;
        }
        
        // Mettre à jour l'icône de signal
        String iconName = "ic_cell_level_" + level;
        int iconResId = getResources().getIdentifier(iconName, "drawable", getPackageName());
        if (iconResId != 0) {
            signalIcon.setImageResource(iconResId);
        }
    }
    
    /**
     * Met à jour le RSSI dans le popup s'il est visible
     */
    private void updatePopupRssi(int rssi) {
        if (bluetoothPopup != null && bluetoothPopup.getVisibility() == android.view.View.VISIBLE && popupDeviceRssi != null) {
            popupDeviceRssi.setText(rssi + " dBm");
            Log.d(TAG, "📶 RSSI popup mis à jour: " + rssi + " dBm");
        }
    }
    
    // ==================== CONNECTION STATUS ====================
    
    private void updateConnectionStatus() {
        if (serviceConnected && bleService != null) {
            if (bleService.isConnected()) {
                // === CONNECTÉ ===
                bluetoothStatusText.setText("Connected");
                
                // Vérifier si les services sont découverts
                if (bleService.areServicesDiscovered()) {
                    // ✅ TOUT BON : Connecté + Services prêts
                    bluetoothServicesStatusText.setText("Services Ready");
                    bluetoothIconBg.setColorFilter(ContextCompat.getColor(this, R.color.connected));
                } else {
                    // ⚠️ CONNECTÉ MAIS PAS LES SERVICES
                    bluetoothServicesStatusText.setText("Initializing...");
                    bluetoothIconBg.setColorFilter(ContextCompat.getColor(this, R.color.warning));
                }
                
                hideWarningPopup();
            } else if (bleService.isScanning()) {
                // === SCAN EN COURS ===
                bluetoothStatusText.setText("Searching...");
                bluetoothServicesStatusText.setText("Services Off");
                bluetoothIconBg.setColorFilter(ContextCompat.getColor(this, R.color.disconnected));
                signalIcon.setImageResource(R.drawable.ic_cell_off);
                showWarningPopup("Searching...");
            } else {
                // === DÉCONNECTÉ ===
                bluetoothStatusText.setText("Disconnected");
                bluetoothServicesStatusText.setText("Services Off");
                bluetoothIconBg.setColorFilter(ContextCompat.getColor(this, R.color.disconnected));
                signalIcon.setImageResource(R.drawable.ic_cell_off);
                showWarningPopup("Idle");
            }
        } else {
            // === SERVICE BLE NON DISPONIBLE ===
            bluetoothStatusText.setText("Disconnected");
            bluetoothServicesStatusText.setText("Services Off");
            bluetoothIconBg.setColorFilter(ContextCompat.getColor(this, R.color.disconnected));
            signalIcon.setImageResource(R.drawable.ic_cell_off);
            showWarningPopup("Service unavailable");
        }
    }
    
    // ==================== WARNING POPUP ====================
    
    private void showWarningPopup(String action) {
        if (warningPopup != null) {
            warningPopup.setVisibility(android.view.View.VISIBLE);
            
            // Update title and message based on context
            if (hasBeenConnectedBefore) {
                // Connection lost - show warning style
                popupTitle.setText("⚠️ Connection Lost");
                popupTitle.setTextColor(ContextCompat.getColor(this, R.color.warning));
                statusMessage.setText("Disconnected");
            } else {
                // First connection - show info style
                popupTitle.setText("📡 Connecting");
                popupTitle.setTextColor(ContextCompat.getColor(this, R.color.white));
                statusMessage.setText("Establishing Connection");
            }
            
            // Update connection status
            if (bleService != null && bleService.isConnected()) {
                connectionStatusValue.setText("Connected");
                connectionStatusValue.setTextColor(ContextCompat.getColor(this, R.color.connected));
            } else {
                connectionStatusValue.setText("Disconnected");
                connectionStatusValue.setTextColor(ContextCompat.getColor(this, R.color.disconnected));
            }
            
            reconnectAttemptsValue.setText(reconnectAttempts + " / " + MAX_RECONNECT_ATTEMPTS);
            actionValue.setText(action);
        }
    }
    
    private void hideWarningPopup() {
        if (warningPopup != null) {
            warningPopup.setVisibility(android.view.View.GONE);
        }
    }
    
    // ==================== PRELOAD LIGHT TAB ====================
    
    private void preloadAllTabs() {
        Log.d(TAG, "preloadAllTabs() appelé - Lancement du thread de préchargement");
        
        // Précharger tous les tabs en arrière-plan dans un thread séparé pour ne pas bloquer
        new Thread(() -> {
            try {
                // Attendre que l'UI principale soit prête (réduit à 200ms)
                Thread.sleep(200);
                
                // Précharger tous les tabs SAUF HOME qui est déjà affiché
                Tab[] tabsToPreload = {Tab.LIGHT, Tab.WATER, Tab.HEATER, Tab.ENERGY, Tab.MULTIMEDIA};
                
                runOnUiThread(() -> {
                    Log.d(TAG, "========================================");
                    Log.d(TAG, "DÉBUT DU PRÉCHARGEMENT DE TOUS LES TABS");
                    Log.d(TAG, "========================================");
                    long totalStartTime = System.currentTimeMillis();
                    
                    for (Tab tab : tabsToPreload) {
                        if (tab == Tab.NONE || tab == currentTab) continue;
                        preloadTab(tab);
                    }
                    
                    long totalEndTime = System.currentTimeMillis();
                    Log.d(TAG, "========================================");
                    Log.d(TAG, "TOUS LES TABS PRÉCHARGÉS EN " + (totalEndTime - totalStartTime) + "ms");
                    Log.d(TAG, "========================================");
                    
                    // Cacher le loading overlay avec une petite animation
                    hideLoadingOverlay();
                });
                
            } catch (InterruptedException e) {
                Log.e(TAG, "Erreur préchargement des tabs: " + e.getMessage());
            }
        }).start();
    }
    
    private void preloadTab(Tab tab) {
        // Si déjà en cache, ne rien faire
        if (cachedTabViews.get(tab.ordinal()) != null) {
            Log.d(TAG, "Tab " + tab.name() + " déjà en cache, skip");
            return;
        }
        
        // Ne jamais précharger le tab actuel
        if (tab == currentTab) {
            Log.d(TAG, "Tab " + tab.name() + " est le tab actuel, skip préchargement");
            return;
        }
        
        Log.d(TAG, "Préchargement du tab " + tab.name() + "...");
        long startTime = System.currentTimeMillis();
        
        try {
            // Inflater la vue
            LayoutInflater inflater = LayoutInflater.from(MainActivity.this);
            View tabView = inflater.inflate(tabLayouts[tab.ordinal()], contentContainer, false);
            
            // Ajouter au container VISIBLE pour forcer le layout/measure
            tabView.setVisibility(View.VISIBLE);
            contentContainer.addView(tabView);
            
            // Forcer le layout immédiat pour que tout soit calculé
            tabView.post(() -> {
                // Une fois que le layout est fait, on peut cacher
                tabView.setVisibility(View.GONE);
            });
            
            // Initialiser le tab manager avec la vue
            BaseTab tabManager = tabManagers.get(tab.ordinal());
            if (tabManager != null) {
                tabManager.initialize(tabView);
                Log.d(TAG, "Tab manager initialisé pour " + tab.name());
            }
            
            // Mettre en cache
            cachedTabViews.put(tab.ordinal(), tabView);
            
            long endTime = System.currentTimeMillis();
            Log.d(TAG, "Tab " + tab.name() + " préchargé en " + (endTime - startTime) + "ms");
            
        } catch (Exception e) {
            Log.e(TAG, "Erreur préchargement tab " + tab.name() + ": " + e.getMessage());
        }
    }


        
    // ==================== AUTO-RECONNECTION ====================
    
    private void scheduleReconnect() {
        if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
            Log.d(TAG, "Limite de reconnexion atteinte");
            runOnUiThread(() -> showWarningPopup("Max attempts reached"));
            return;
        }
        
        reconnectAttempts++;
        Log.d(TAG, "Reconnexion automatique programmée (tentative " + reconnectAttempts + "/" + MAX_RECONNECT_ATTEMPTS + ")");
        
        // Mettre à jour le popup
        runOnUiThread(() -> showWarningPopup("Retrying in 5s..."));
        
        reconnectRunnable = () -> {
            if (serviceConnected && bleService != null && !bleService.isConnected()) {
                Log.d(TAG, "Tentative reconnexion après nettoyage complet");
                runOnUiThread(() -> showWarningPopup("Cleaning up..."));
                
                // Attendre un peu plus avant de scanner pour laisser le BLE se nettoyer
                reconnectHandler.postDelayed(() -> {
                    if (serviceConnected && bleService != null && !bleService.isConnected()) {
                        runOnUiThread(() -> showWarningPopup("Searching..."));
                        bleService.startScan();
                    }
                }, 1000); // Délai supplémentaire de 1s avant le scan
            }
        };
        
        reconnectHandler.postDelayed(reconnectRunnable, RECONNECT_DELAY_MS);
    }
    
    private void cancelReconnectTimer() {
        if (reconnectRunnable != null) {
            reconnectHandler.removeCallbacks(reconnectRunnable);
            reconnectRunnable = null;
        }
    }
    
    private void resetReconnectAttempts() {
        reconnectAttempts = 0;
        cancelReconnectTimer();
        runOnUiThread(() -> hideWarningPopup());
    }
    
    // ==================== LOADING OVERLAY ====================
    
    private void hideLoadingOverlay() {
        if (loadingOverlay != null) {
            // Désactiver le blocage des clics immédiatement
            loadingOverlay.setClickable(false);
            
            // Animation de fade out
            loadingOverlay.animate()
                .alpha(0f)
                .setDuration(300)
                .withEndAction(() -> {
                    loadingOverlay.setVisibility(View.GONE);
                    Log.d(TAG, "Loading overlay caché");
                })
                .start();
        }
    }
}
