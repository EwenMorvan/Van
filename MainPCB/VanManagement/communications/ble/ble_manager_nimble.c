/**
 * @file ble_manager_nimble.c
 * @brief BLE Manager with dual role support (Peripheral + Central)
 * 
 * Features:
 * - Peripheral role: Advertises as "VanManager" for mobile app connections
 * - Central role: Connects to external BLE devices (e.g., battery monitors)
 * - Automatic fragmentation for large data transfers
 * - XiaoXiang/JBD BMS protocol support for battery monitoring
 */

#include "ble_manager_nimble.h"

static const char *TAG = "\033[0;34mBLE_MGR\033[0m";

// ============================================================================
// CONFIGURATION
// ============================================================================

// MAX_CONNECTIONS: Limite le nombre total de connexions BLE simultanées
// Note: Ce nombre inclut TOUS les appareils (apps mobiles + appareils externes)
// Configuration recommandée: 3 apps mobiles max + appareils externes (batteries, etc.)
#define MAX_CONNECTIONS 4                    // Max BLE connections (apps + external devices)
#define BLE_MAX_FRAGMENT_SIZE 500           // Max bytes per BLE packet
#define FRAGMENT_DELAY_MS 20               // Delay between fragments (increased for multi-app stability)
#define MAX_DEVICE_NAME_LEN 32              // Max device name length
#define MAX_EXTERNAL_DEVICES 8              // Max external devices (batteries, etc.)
#define DEBUG_LOG_ALL_SCANNED_DEVICES 0     // Set to 1 to log all scanned BLE devices

// Service UUIDs
#define VAN_SERVICE_UUID_16         0xAAA0
#define VAN_CHAR_COMMAND_UUID_16    0xAAA1  
#define VAN_CHAR_STATE_UUID_16      0xAAA2

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    uint16_t conn_handle;
    bool connected;
    bool notifications_enabled;  // Flag pour savoir si le client a activé les notifications
    uint32_t notifications_enabled_time;  // Timestamp quand les notifications ont été activées
    uint8_t mac[6];
} ble_connection_t;

typedef struct {
    uint8_t mac[6];
    char device_name[MAX_DEVICE_NAME_LEN];
    bool registered;
    bool connected;
    uint16_t conn_handle;
    uint8_t received_data[256];
    size_t received_len;
    bool connecting;
    
    // GATT discovery state
    uint16_t notify_handles[16];
    uint16_t indicate_handles[16];
    uint8_t notify_count;
    uint8_t indicate_count;
    bool subscriptions_done;
    uint8_t services_discovered;
    
    // JBD BMS protocol handles (XiaoXiang battery monitor)
    uint16_t jbd_tx_handle;      // Handle 21: Send commands to BMS
    uint16_t jbd_rx_handle;      // Handle 16: Receive notifications from BMS
    bool jbd_ready;
    
    // Response reassembly (BLE packets are fragmented)
    uint8_t jbd_buffer[64];
    size_t jbd_buffer_len;
    uint32_t jbd_last_fragment_time;
    // Flag set when service 0xff00 (JBD BMS) is detected for this device
    bool is_jbd_service;
    // Videoprojector specific handles (service 0x181A)
    bool is_projector;
    uint16_t proj_control_handle; // 0x2A58 write
    uint16_t proj_status_handle;  // 0x2A19 read/notify
} external_device_t;

// ============================================================================
// GLOBAL STATE
// ============================================================================

static ble_connection_t g_connections[MAX_CONNECTIONS] = {0};
static external_device_t g_external_devices[MAX_EXTERNAL_DEVICES] = {0};
static ble_receive_callback_t g_receive_callback = NULL;
static SemaphoreHandle_t g_ble_mutex = NULL;
static bool g_ble_initialized = false;

// GATT Characteristic handles
static uint16_t g_char_command_handle = 0;
static uint16_t g_char_state_handle = 0;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static void van_advertise(void);
static int van_gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
static int van_gap_event(struct ble_gap_event *event, void *arg);
static void van_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
static int external_device_gap_event(struct ble_gap_event *event, void *arg);
static void start_scan_for_external_devices(void);

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static void lock_ble(void) {
    if (g_ble_mutex) {
        xSemaphoreTake(g_ble_mutex, portMAX_DELAY);
    }
}

static void unlock_ble(void) {
    if (g_ble_mutex) {
        xSemaphoreGive(g_ble_mutex);
    }
}

static int find_free_connection_slot(void) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!g_connections[i].connected) {
            return i;
        }
    }
    return -1;
}

static int find_connection_by_handle(uint16_t handle) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_connections[i].connected && g_connections[i].conn_handle == handle) {
            return i;
        }
    }
    return -1;
}

static external_device_t* find_external_device_by_mac(const uint8_t mac[6]) {
    for (int i = 0; i < MAX_EXTERNAL_DEVICES; i++) {
        if (g_external_devices[i].registered &&
            memcmp(g_external_devices[i].mac, mac, 6) == 0) {
            return &g_external_devices[i];
        }
    }
    return NULL;
}

static external_device_t* find_free_external_device(void) {
    for (int i = 0; i < MAX_EXTERNAL_DEVICES; i++) {
        if (!g_external_devices[i].registered) {
            return &g_external_devices[i];
        }
    }
    return NULL;
}

// ============================================================================
// GATT CALLBACKS
// ============================================================================

static void van_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    char buf[BLE_UUID_STR_LEN];
    
    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGI(TAG, "Registered service %s with handle=%d", 
                     ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
            break;
            
        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGI(TAG, "Registered characteristic %s with val_handle=%d", 
                     ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf), ctxt->chr.val_handle);
            
            uint16_t uuid = ble_uuid_u16(ctxt->chr.chr_def->uuid);
            if (uuid == VAN_CHAR_COMMAND_UUID_16) {
                g_char_command_handle = ctxt->chr.val_handle;
            } else if (uuid == VAN_CHAR_STATE_UUID_16) {
                g_char_state_handle = ctxt->chr.val_handle;
            }
            break;
            
        default:
            break;
    }
}

static int van_gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            if (attr_handle == g_char_command_handle) {
                uint16_t data_len = ctxt->om->om_len;
                uint8_t *data = malloc(data_len);
                if (data) {
                    ble_hs_mbuf_to_flat(ctxt->om, data, data_len, NULL);
                    
                    ESP_LOGI(TAG, "📱 Data received from app (%d bytes) on conn_handle=%d", data_len, conn_handle);
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, data_len, ESP_LOG_INFO);
                    
                    if (g_receive_callback) {
                        g_receive_callback(conn_handle, data, data_len);
                    }
                    
                    free(data);
                }
            }
            break;
            
        case BLE_GATT_ACCESS_OP_READ_CHR:
            // Handle reads if needed
            break;
            
        default:
            break;
    }
    
    return 0;
}

// ============================================================================
// GATT SERVICE DEFINITIONS
// ============================================================================

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        // Van Management Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(VAN_SERVICE_UUID_16),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Command characteristic (write - for receiving data from app)
                .uuid = BLE_UUID16_DECLARE(VAN_CHAR_COMMAND_UUID_16),
                .access_cb = van_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &g_char_command_handle,
            },
            {
                // State characteristic (notify - for sending data to app)
                .uuid = BLE_UUID16_DECLARE(VAN_CHAR_STATE_UUID_16),
                .access_cb = van_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &g_char_state_handle,
            },
            {0} // End
        },
    },
    {0} // End of services
};

// ============================================================================
// GAP EVENT HANDLER
// ============================================================================

static int van_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT: {
            ESP_LOGI(TAG, "Connection %s", event->connect.status == 0 ? "established" : "failed");
            
            if (event->connect.status == 0) {
                lock_ble();
                int slot = find_free_connection_slot();
                if (slot >= 0) {
                    g_connections[slot].conn_handle = event->connect.conn_handle;
                    g_connections[slot].connected = true;
                    g_connections[slot].notifications_enabled = false;  // Pas encore prêt
                    
                    struct ble_gap_conn_desc desc;
                    if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
                        memcpy(g_connections[slot].mac, desc.peer_id_addr.val, 6);
                        ESP_LOGI(TAG, "📱 Device connected [%02X:%02X:%02X:%02X:%02X:%02X]",
                                 desc.peer_id_addr.val[0], desc.peer_id_addr.val[1],
                                 desc.peer_id_addr.val[2], desc.peer_id_addr.val[3],
                                 desc.peer_id_addr.val[4], desc.peer_id_addr.val[5]);
                    }
                    
                    ble_att_set_preferred_mtu(512);
                    
                    // Compter les connexions actuelles
                    int connected_count = 0;
                    for (int i = 0; i < MAX_CONNECTIONS; i++) {
                        if (g_connections[i].connected) {
                            connected_count++;
                        }
                    }
                    
                    ESP_LOGI(TAG, "✅ Total connected: %d/%d", connected_count, MAX_CONNECTIONS);
                    
                    // IMPORTANT: Redémarrer l'advertising pour permettre d'autres connexions
                    // (seulement si on n'a pas atteint la limite)
                    if (connected_count < MAX_CONNECTIONS) {
                        unlock_ble();
                        vTaskDelay(pdMS_TO_TICKS(100));  // Petit délai pour stabiliser
                        van_advertise();
                        ESP_LOGI(TAG, "🔄 Advertising restarted (accepting more connections)");
                        lock_ble();
                    } else {
                        ESP_LOGI(TAG, "🛑 Max connections reached, advertising stopped");
                    }
                } else {
                    ESP_LOGW(TAG, "No free slots, disconnecting");
                    ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                }
                unlock_ble();
            } else {
                van_advertise();
            }
            break;
        }
            
        case BLE_GAP_EVENT_DISCONNECT: {
            ESP_LOGI(TAG, "Device disconnected");
            
            lock_ble();
            int slot = find_connection_by_handle(event->disconnect.conn.conn_handle);
            if (slot >= 0) {
                g_connections[slot].connected = false;
                g_connections[slot].notifications_enabled = false;
                memset(g_connections[slot].mac, 0, 6);
            }
            unlock_ble();
            
            van_advertise();
            break;
        }
            
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated: %d", event->mtu.value);
            break;
            
        case BLE_GAP_EVENT_ADV_COMPLETE:
            van_advertise();
            break;
            
        case BLE_GAP_EVENT_SUBSCRIBE: {
            ESP_LOGI(TAG, "Client %s notifications", 
                     event->subscribe.cur_notify ? "enabled" : "disabled");
            
            // Mettre à jour le flag pour cette connexion
            lock_ble();
            int slot = find_connection_by_handle(event->subscribe.conn_handle);
            if (slot >= 0) {
                g_connections[slot].notifications_enabled = event->subscribe.cur_notify;
                if (event->subscribe.cur_notify) {
                    g_connections[slot].notifications_enabled_time = xTaskGetTickCount();
                    ESP_LOGI(TAG, "✅ Client [slot %d] is now ready to receive data", slot);
                }
            }
            unlock_ble();
            break;
        }
    }
    
    return 0;
}

// ============================================================================
// ADVERTISING
// ============================================================================

static void van_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    
    // Stop any existing advertising first to avoid rc=2 (BLE_HS_EALREADY)
    ble_gap_adv_stop();
    
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t*)"VanManager";
    fields.name_len = strlen("VanManager");
    fields.name_is_complete = 1;
    
    ble_gap_adv_set_fields(&fields);
    
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    // Faster advertising intervals for better discovery
    adv_params.itvl_min = 160;   // 100ms (units of 0.625ms)
    adv_params.itvl_max = 320;   // 200ms
    
    int rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, van_gap_event, NULL);
    
    if (rc == 0) {
        ESP_LOGI(TAG, "📡 Advertising started");
    } else if (rc == BLE_HS_EALREADY) {
        ESP_LOGD(TAG, "Advertising already active (rc=2), this is normal");
    } else {
        ESP_LOGE(TAG, "Failed to start advertising; rc=%d", rc);
    }
}

// ============================================================================
// EXTERNAL DEVICE SCANNING & CONNECTION (CENTRAL ROLE)
// ============================================================================

// Callback for GATT characteristic discovery
static int on_characteristic_discovered(uint16_t conn_handle,
                                        const struct ble_gatt_error *error,
                                        const struct ble_gatt_chr *chr,
                                        void *arg) {
    external_device_t *dev = (external_device_t *)arg;
    
    if (error->status == 0 && chr != NULL) {
        char uuid_str[BLE_UUID_STR_LEN];
        ble_uuid_to_str(&chr->uuid.u, uuid_str);
        ESP_LOGI(TAG, "📋 Char UUID=%s, handle=%d, flags=0x%02x", 
                 uuid_str, chr->val_handle, chr->properties);
        
        // Check for XiaoXiang/JBD BMS specific characteristics
        if (chr->uuid.u.type == BLE_UUID_TYPE_16) {
            uint16_t uuid16 = chr->uuid.u16.value;
            if (uuid16 == 0xFF01) {
                // RX characteristic (notifications from BMS)
                dev->jbd_rx_handle = chr->val_handle;
                ESP_LOGI(TAG, "🔋 Found JBD RX characteristic (0xFF01) at handle=%d", chr->val_handle);
            } else if (uuid16 == 0xFF02) {
                // TX characteristic (commands to BMS)
                dev->jbd_tx_handle = chr->val_handle;
                ESP_LOGI(TAG, "🔋 Found JBD TX characteristic (0xFF02) at handle=%d", chr->val_handle);
            } else if (uuid16 == 0x2A58) {
                // Video projector control characteristic (write)
                dev->is_projector = true;
                dev->proj_control_handle = chr->val_handle;
                ESP_LOGI(TAG, "📽️ Found Projector CONTROL char (0x2A58) at handle=%d", chr->val_handle);
            } else if (uuid16 == 0x2A19) {
                // Video projector status characteristic (read/notify)
                dev->is_projector = true;
                dev->proj_status_handle = chr->val_handle;
                ESP_LOGI(TAG, "📽️ Found Projector STATUS char (0x2A19) at handle=%d", chr->val_handle);
            }
        }
        
        // Store handles for deferred subscription (after discovery completes)
        if (chr->properties & BLE_GATT_CHR_PROP_NOTIFY) {
            if (dev->notify_count < 16) {
                dev->notify_handles[dev->notify_count++] = chr->val_handle;
                ESP_LOGI(TAG, "📝 Stored notify handle=%d (count=%d)", 
                         chr->val_handle, dev->notify_count);
            }
        }
        
        if (chr->properties & BLE_GATT_CHR_PROP_INDICATE) {
            if (dev->indicate_count < 16) {
                dev->indicate_handles[dev->indicate_count++] = chr->val_handle;
                ESP_LOGI(TAG, "📝 Stored indicate handle=%d (count=%d)", 
                         chr->val_handle, dev->indicate_count);
            }
        }
    } else if (error->status == BLE_HS_EDONE) {
        // Normal completion - service has no more characteristics
        ESP_LOGD(TAG, "  (characteristic discovery complete for this service)");
    } else {
        // Other errors (status=3 is ENOMEM, status=10 is ENOTCONN - both can be transient)
        ESP_LOGW(TAG, "  ⚠️ Characteristic discovery status=%d (may be normal)", error->status);
    }
    
    return 0;
}

// Callback for GATT read responses
// Note: Fonction non utilisée actuellement, gardée pour référence future
#if 0
static int on_characteristic_read(uint16_t conn_handle,
                                  const struct ble_gatt_error *error,
                                  struct ble_gatt_attr *attr,
                                  void *arg) {
    if (error->status == 0) {
        ESP_LOGI(TAG, "  ✅ Read handle=%d: %d bytes", attr->handle, attr->om->om_len);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, attr->om->om_data, attr->om->om_len, ESP_LOG_INFO);
        
        // Store data in device structure
        for (int i = 0; i < MAX_EXTERNAL_DEVICES; i++) {
            if (g_external_devices[i].registered && 
                g_external_devices[i].connected && 
                g_external_devices[i].conn_handle == conn_handle) {
                
                lock_ble();
                uint16_t len = attr->om->om_len;
                if (len > sizeof(g_external_devices[i].received_data)) {
                    len = sizeof(g_external_devices[i].received_data);
                }
                memcpy(g_external_devices[i].received_data, attr->om->om_data, len);
                g_external_devices[i].received_len = len;
                unlock_ble();
                break;
            }
        }
    } else {
        ESP_LOGW(TAG, "  ⚠️ Read handle failed: status=%d", error->status);
    }
    return 0;
}
#endif

// Read callback used for projector (and other simple reads)
static int on_simple_read(uint16_t conn_handle,
                          const struct ble_gatt_error *error,
                          struct ble_gatt_attr *attr,
                          void *arg) {
    external_device_t *dev = (external_device_t *)arg;
    if (error->status == 0 && attr && attr->om) {
        uint16_t len = attr->om->om_len;
        if (len > sizeof(dev->received_data)) len = sizeof(dev->received_data);
        ble_hs_mbuf_to_flat(attr->om, dev->received_data, len, NULL);
        lock_ble();
        dev->received_len = len;
        unlock_ble();
        ESP_LOGI(TAG, "📥 Read response from %s handle=%d (%d bytes)", dev->device_name, attr->handle, len);
    } else {
        ESP_LOGW(TAG, "📥 Read failed or empty (status=%d)", error->status);
    }
    return 0;
}

// Subscribe to all stored characteristics (called after discovery completes)
static void subscribe_to_characteristics(uint16_t conn_handle, external_device_t *dev) {
    ESP_LOGI(TAG, "🔔 Subscribing to %d notifications and %d indications...", 
             dev->notify_count, dev->indicate_count);
    
    // Subscribe to notifications
    for (int i = 0; i < dev->notify_count; i++) {
        uint16_t val_handle = dev->notify_handles[i];
        uint16_t cccd_handle = val_handle + 1;  // CCCD is usually next handle
        uint8_t value[2] = {0x01, 0x00};  // Enable notifications
        
        ESP_LOGI(TAG, "  → Subscribing to notifications on handle=%d (CCCD=%d)", 
                 val_handle, cccd_handle);
        
        int rc = ble_gattc_write_flat(conn_handle, cccd_handle, 
                                      value, sizeof(value), NULL, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "    ❌ Failed to subscribe: rc=%d", rc);
        } else {
            ESP_LOGI(TAG, "    ✅ Subscription request sent");
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));  // Small delay between subscriptions
    }
    
    // Subscribe to indications
    for (int i = 0; i < dev->indicate_count; i++) {
        uint16_t val_handle = dev->indicate_handles[i];
        uint16_t cccd_handle = val_handle + 1;
        uint8_t value[2] = {0x02, 0x00};  // Enable indications
        
        ESP_LOGI(TAG, "  → Subscribing to indications on handle=%d (CCCD=%d)", 
                 val_handle, cccd_handle);
        
        int rc = ble_gattc_write_flat(conn_handle, cccd_handle, 
                                      value, sizeof(value), NULL, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "    ❌ Failed to subscribe: rc=%d", rc);
        } else {
            ESP_LOGI(TAG, "    ✅ Subscription request sent");
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    ESP_LOGI(TAG, "✅ All subscription requests sent");
    
    // If this device exposes the JBD service (0xFF00) then apply the
    // JBD-specific workaround (static handles for RX/TX). Otherwise skip it.
    if (dev->is_jbd_service) {
        // WORKAROUND: JBD BMS doesn't respond to characteristic discovery for service 0xff00
        // Testing confirmed: Handle 21 = TX (send commands), Handle 16 = RX (receive notifications)
        ESP_LOGI(TAG, "🔧 Setting up JBD handles for service 0xff00 (handles 15-22)");

        // Subscribe to handle 16 for notifications (RX from battery)
        dev->jbd_rx_handle = 16;
        uint8_t notify_value[2] = {0x01, 0x00};  // Enable notifications
        int rc = ble_gattc_write_flat(conn_handle, 17, notify_value, sizeof(notify_value), NULL, NULL);  // 17 = CCCD for handle 16
        if (rc == 0) {
            ESP_LOGI(TAG, "🔔 Subscribed to notifications on handle 16 (JBD RX)");
        } else {
            ESP_LOGW(TAG, "⚠️ Failed to subscribe to handle 16: rc=%d", rc);
        }

        // Set TX handle to 21 (confirmed working from tests)
        dev->jbd_tx_handle = 21;
        dev->jbd_ready = true;
        dev->jbd_buffer_len = 0;  // Initialize reassembly buffer
        ESP_LOGI(TAG, "🔋 JBD RX handle: %d (notifications)", dev->jbd_rx_handle);
        ESP_LOGI(TAG, "🔋 JBD TX handle: %d (commands) ✅ CONFIRMED", dev->jbd_tx_handle);
        ESP_LOGI(TAG, "🔋 Battery data available on handle 16 (use ble_request_battery_update to poll)");
    } else {
        ESP_LOGI(TAG, "🔎 No JBD service detected for %s — skipping JBD workaround", dev->device_name);
    }
}

// Callback for GATT service discovery
static int on_service_discovered(uint16_t conn_handle,
                                 const struct ble_gatt_error *error,
                                 const struct ble_gatt_svc *service,
                                 void *arg) {
    external_device_t *dev = (external_device_t *)arg;
    
    if (error->status == 0) {
        char uuid_str[BLE_UUID_STR_LEN];
        ble_uuid_to_str(&service->uuid.u, uuid_str);
        ESP_LOGI(TAG, "📦 Service discovered: UUID=%s (handles %d-%d)", 
                 uuid_str, service->start_handle, service->end_handle);
        
        dev->services_discovered++;
        // Detect JBD service (0xFF00) and mark device accordingly
        if (service->uuid.u.type == BLE_UUID_TYPE_16) {
            uint16_t svc_uuid16 = service->uuid.u16.value;
            if (svc_uuid16 == 0xFF00) {
                dev->is_jbd_service = true;
                ESP_LOGI(TAG, "🔎 JBD service (0xFF00) detected for %s", dev->device_name);
            }
        }
        
        // Discover all characteristics in this service
        int rc = ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle,
                                         on_characteristic_discovered, arg);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to discover characteristics for service %s: rc=%d", uuid_str, rc);
        }
    } else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "✅ Service discovery complete for %s (%d services found)", 
                 dev->device_name, dev->services_discovered);
        
        // Subscribe to notifications once discovery is complete
        if (!dev->subscriptions_done) {
            dev->subscriptions_done = true;
            subscribe_to_characteristics(conn_handle, dev);
        }
    }
    
    return 0;
}

static int external_device_gap_event(struct ble_gap_event *event, void *arg) {
    external_device_t *dev = (external_device_t *)arg;
    
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "🔋 External device connected: %s", dev->device_name);
                lock_ble();
                dev->connected = true;
                dev->connecting = false;
                dev->conn_handle = event->connect.conn_handle;
                // Reset discovery state for new connection
                dev->notify_count = 0;
                dev->indicate_count = 0;
                dev->subscriptions_done = false;
                dev->services_discovered = 0;
                unlock_ble();
                
                // Request MTU update: 50 bytes for projector (detected by name), 512 for other devices
                bool is_projector_device = (strstr(dev->device_name, "Projector") != NULL || 
                                           strstr(dev->device_name, "VideoProjector") != NULL);
                uint16_t preferred_mtu = is_projector_device ? 50 : 512;
                ble_att_set_preferred_mtu(preferred_mtu);
                int rc_mtu = ble_gattc_exchange_mtu(event->connect.conn_handle, NULL, NULL);
                if (rc_mtu == 0) {
                    ESP_LOGI(TAG, "📏 MTU exchange initiated (requesting %d bytes for %s)", 
                             preferred_mtu, is_projector_device ? "projector" : "device");
                } else {
                    ESP_LOGW(TAG, "⚠️ MTU exchange failed: rc=%d", rc_mtu);
                }
                
                // Discover all services and subscribe to notifications
                ESP_LOGI(TAG, "🔍 Starting service discovery...");
                int rc = ble_gattc_disc_all_svcs(event->connect.conn_handle, 
                                                  on_service_discovered, dev);
                if (rc != 0) {
                    ESP_LOGE(TAG, "Failed to start service discovery: rc=%d", rc);
                }
            } else {
                ESP_LOGE(TAG, "Connection to %s failed: status=%d", 
                         dev->device_name, event->connect.status);
                lock_ble();
                dev->connected = false;
                dev->connecting = false;
                unlock_ble();
            }
            break;
            
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "🔋 External device disconnected: %s", dev->device_name);
            lock_ble();
            dev->connected = false;
            dev->connecting = false;
            dev->conn_handle = 0;
            unlock_ble();
            
            // Retry connection after delay
            vTaskDelay(pdMS_TO_TICKS(5000));
            start_scan_for_external_devices();
            break;
            
        case BLE_GAP_EVENT_NOTIFY_RX: {
            // Received notification from external device (JBD BMS responses or others)
            uint16_t len = event->notify_rx.om->om_len;
            uint8_t fragment[50];  // Increased buffer size to handle larger notifications
            
            if (len > sizeof(fragment)) len = sizeof(fragment);
            ble_hs_mbuf_to_flat(event->notify_rx.om, fragment, len, NULL);
            
            lock_ble();
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Reset buffer if timeout (fragmentation window expired)
            // If device is JBD, do fragment reassembly; otherwise handle simple notifications
            if (dev->is_jbd_service) {
                if (dev->jbd_buffer_len > 0 && (now - dev->jbd_last_fragment_time) > 1000) {
                    dev->jbd_buffer_len = 0;
                }

                // Append fragment to reassembly buffer
                if (dev->jbd_buffer_len + len <= sizeof(dev->jbd_buffer)) {
                    memcpy(dev->jbd_buffer + dev->jbd_buffer_len, fragment, len);
                    dev->jbd_buffer_len += len;
                    dev->jbd_last_fragment_time = now;

                    // Check if complete JBD response received (DD...77)
                    if (dev->jbd_buffer_len >= 7 && 
                        dev->jbd_buffer[0] == 0xDD && 
                        dev->jbd_buffer[dev->jbd_buffer_len - 1] == 0x77) {

                        // Copy complete response for parsing
                        memcpy(dev->received_data, dev->jbd_buffer, dev->jbd_buffer_len);
                        dev->received_len = dev->jbd_buffer_len;
                        dev->jbd_buffer_len = 0;  // Reset for next response
                    }
                } else {
                    dev->jbd_buffer_len = 0;  // Overflow, reset
                }
            } else {
                // Non-JBD device: copy notification payload directly for consumer to read
                size_t copy_len = len;
                if (copy_len > sizeof(dev->received_data)) copy_len = sizeof(dev->received_data);
                memcpy(dev->received_data, fragment, copy_len);
                dev->received_len = copy_len;
                dev->jbd_last_fragment_time = now;
                ESP_LOGI(TAG, "🔔 Notification received from %s: %d bytes", dev->device_name, copy_len);
            }
            unlock_ble();
            break;
        }
            
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated for %s: %d", dev->device_name, event->mtu.value);
            break;
    }
    
    return 0;
}

static int scan_event_handler(struct ble_gap_event *event, void *arg) {
    if (event->type != BLE_GAP_EVENT_DISC) {
        return 0;
    }
    
    // Debug logging (enable with DEBUG_LOG_ALL_SCANNED_DEVICES)
#if DEBUG_LOG_ALL_SCANNED_DEVICES
    ESP_LOGI(TAG, "📡 [%02X:%02X:%02X:%02X:%02X:%02X] type=%d RSSI=%d",
             event->disc.addr.val[0], event->disc.addr.val[1],
             event->disc.addr.val[2], event->disc.addr.val[3],
             event->disc.addr.val[4], event->disc.addr.val[5],
             event->disc.addr.type, event->disc.rssi);
#endif
    
    // Check if this device matches any registered external device
    lock_ble();
    for (int i = 0; i < MAX_EXTERNAL_DEVICES; i++) {
        if (!g_external_devices[i].registered || 
            g_external_devices[i].connected || 
            g_external_devices[i].connecting) {
            continue;
        }
        
        if (memcmp(event->disc.addr.val, g_external_devices[i].mac, 6) == 0) {
            ESP_LOGI(TAG, "🔍 Found registered device: %s [%02X:%02X:%02X:%02X:%02X:%02X]",
                     g_external_devices[i].device_name,
                     event->disc.addr.val[0], event->disc.addr.val[1],
                     event->disc.addr.val[2], event->disc.addr.val[3],
                     event->disc.addr.val[4], event->disc.addr.val[5]);
            
            g_external_devices[i].connecting = true;
            
            // Stop scan
            ble_gap_disc_cancel();
            
            // Initiate connection
            struct ble_gap_conn_params conn_params = {
                .scan_itvl = 0x0010,
                .scan_window = 0x0010,
                .itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN,
                .itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX,
                .latency = 0,
                .supervision_timeout = 0x0100,
                .min_ce_len = 0,
                .max_ce_len = 0,
            };
            
            int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr,
                                     30000, &conn_params, external_device_gap_event,
                                     &g_external_devices[i]);
            
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to connect to %s: rc=%d",
                         g_external_devices[i].device_name, rc);
                g_external_devices[i].connecting = false;
            }
            
            unlock_ble();
            return 0;
        }
    }
    unlock_ble();
    
    return 0;
}

static void start_scan_for_external_devices(void) {
    // Check if any registered devices need connection
    lock_ble();
    bool need_scan = false;
    int devices_to_connect = 0;
    for (int i = 0; i < MAX_EXTERNAL_DEVICES; i++) {
        if (g_external_devices[i].registered) {
            ESP_LOGI(TAG, "Device %d: %s - registered=%d, connected=%d, connecting=%d",
                     i, g_external_devices[i].device_name,
                     g_external_devices[i].registered,
                     g_external_devices[i].connected,
                     g_external_devices[i].connecting);
            
            if (!g_external_devices[i].connected && !g_external_devices[i].connecting) {
                need_scan = true;
                devices_to_connect++;
            }
        }
    }
    unlock_ble();
    
    if (!need_scan) {
        ESP_LOGI(TAG, "No devices need scanning (all connected or connecting)");
        return;
    }
    
    ESP_LOGI(TAG, "🔍 Starting scan for %d external device(s)...", devices_to_connect);
    
    // Optimized scan parameters to reduce interference with RMT/LEDs
    struct ble_gap_disc_params disc_params = {
        .itvl = 0x60,          // Longer scan interval (60ms instead of 10ms)
        .window = 0x30,        // Reduced scan window (30ms)
        .filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
        .limited = 0,
        .passive = 1,          // Passive scan to reduce load
        .filter_duplicates = 1,
    };
    
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params,
                          scan_event_handler, NULL);
    
    if (rc != 0) {
        if (rc == BLE_HS_EALREADY) {
            ESP_LOGI(TAG, "Scan already in progress");
        } else {
            ESP_LOGE(TAG, "Failed to start scan: rc=%d", rc);
        }
    } else {
        ESP_LOGI(TAG, "✅ Scan started successfully");
    }
}

// ============================================================================
// BLE STACK INITIALIZATION
// ============================================================================

static void ble_app_on_sync(void) {
    ESP_LOGI(TAG, "BLE stack synchronized");
    
    ble_hs_util_ensure_addr(0);
    ble_svc_gap_device_name_set("VanManager");
    
    ble_gatts_count_cfg(gatt_services);
    ble_gatts_add_svcs(gatt_services);
    ble_gatts_start();
    
    ESP_LOGI(TAG, "GATT services registered");
    
    van_advertise();
    
    // Don't start scan automatically to avoid LED interference
    // Scan will start manually when a device is added
    ESP_LOGI(TAG, "💡 Scan not started (will start when device added)");
}

static void ble_app_on_reset(int reason) {
    ESP_LOGW(TAG, "BLE stack reset, reason=%d", reason);
}

static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE host task started on CPU%d", xPortGetCoreID());
    nimble_port_run();
}

// ============================================================================
// PUBLIC API - INITIALIZATION
// ============================================================================

esp_err_t ble_init(ble_receive_callback_t receive_callback) {
    if (g_ble_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing BLE Manager...");
    
    g_ble_mutex = xSemaphoreCreateMutex();
    if (!g_ble_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }
    
    g_receive_callback = receive_callback;
    
    memset(g_connections, 0, sizeof(g_connections));
    memset(g_external_devices, 0, sizeof(g_external_devices));
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize NimBLE stack
    nimble_port_init();
    
    // Configuration pour supporter plusieurs connexions simultanées
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.reset_cb = ble_app_on_reset;
    ble_hs_cfg.gatts_register_cb = van_gatt_register_cb;
    
    // Note: Le nombre max de connexions est configuré via menuconfig
    // CONFIG_BT_NIMBLE_MAX_CONNECTIONS doit être défini à 4 dans sdkconfig.defaults
    ESP_LOGI(TAG, "⚙️  BLE configured for %d simultaneous connections", MAX_CONNECTIONS);
    
    // Pin BLE task to CPU1 to avoid blocking main application (CPU0)
    // Lower priority (3) to prevent interference with LED animations (priority 6)
    ESP_LOGI(TAG, "📌 Creating BLE task pinned to CPU1...");
    xTaskCreatePinnedToCore(
        ble_host_task,           // Task function
        "nimble_host",           // Task name
        4096,                    // Stack size
        NULL,                    // Parameters
        3,                       // Priority (lower than LEDs)
        NULL,                    // Task handle
        1                        // CPU1
    );
    
    g_ble_initialized = true;
    ESP_LOGI(TAG, "✅ BLE Manager initialized on CPU1");
    
    return ESP_OK;
}

bool ble_is_connected(void) {
    lock_ble();
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_connections[i].connected) {
            unlock_ble();
            return true;
        }
    }
    unlock_ble();
    return false;
}

uint8_t ble_get_connection_count(void) {
    lock_ble();
    uint8_t count = 0;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_connections[i].connected) {
            count++;
        }
    }
    unlock_ble();
    return count;
}

// ============================================================================
// PUBLIC API - DATA TRANSMISSION
// ============================================================================

esp_err_t ble_send_raw(const uint8_t* data, size_t length) {
    if (!g_ble_initialized || !data || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_char_state_handle == 0) {
        ESP_LOGW(TAG, "State handle not ready");
        return ESP_ERR_INVALID_STATE;
    }
    
    lock_ble();
    
    // Compter le nombre d'apps connectées ET prêtes (notifications activées)
    uint8_t app_count = 0;
    uint8_t ready_count = 0;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_connections[i].connected) {
            app_count++;
            if (g_connections[i].notifications_enabled) {
                ready_count++;
            }
        }
    }
    
    if (ready_count == 0) {
        unlock_ble();
        ESP_LOGD(TAG, "⏳ No clients ready to receive data yet (%d connected but notifications not enabled)", app_count);
        return ESP_OK;  // Pas d'erreur, juste pas encore prêt
    }
    
    bool needs_fragmentation = length > BLE_MAX_FRAGMENT_SIZE;
    int num_fragments = needs_fragmentation ? 
                        ((length + BLE_MAX_FRAGMENT_SIZE - 1) / BLE_MAX_FRAGMENT_SIZE) : 1;
    
    if (needs_fragmentation) {
        ESP_LOGD(TAG, "📦 Fragmenting %d bytes into %d parts for %d ready app(s)", (int)length, num_fragments, ready_count);
    } else {
        ESP_LOGD(TAG, "📤 Sending %d bytes to %d ready app(s)", (int)length, ready_count);
    }
    
    esp_err_t result = ESP_OK;
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!g_connections[i].connected || !g_connections[i].notifications_enabled) {
            continue;  // Ignorer les connexions pas prêtes
        }
        
        // Attendre au moins 200ms après l'activation des notifications avant d'envoyer
        // Cela laisse le temps au stack BLE de se stabiliser
        uint32_t time_since_enabled = (xTaskGetTickCount() - g_connections[i].notifications_enabled_time) * portTICK_PERIOD_MS;
        if (time_since_enabled < 200) {
            ESP_LOGD(TAG, "  ⏳ Slot %d not ready yet (only %lums since notifications enabled, waiting...)", 
                     i, time_since_enabled);
            continue;
        }
        
        ESP_LOGD(TAG, "  → Sending to slot %d (conn_handle=%d)", i, g_connections[i].conn_handle);
        
        if (!needs_fragmentation) {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
            if (om) {
                int rc = ble_gatts_notify_custom(g_connections[i].conn_handle, 
                                                  g_char_state_handle, om);
                if (rc != 0) {
                    ESP_LOGE(TAG, "Send failed for slot %d (conn_handle=%d); rc=%d", 
                             i, g_connections[i].conn_handle, rc);
                    result = ESP_FAIL;
                }
            }
        } else {
            for (int frag = 0; frag < num_fragments; frag++) {
                size_t offset = frag * BLE_MAX_FRAGMENT_SIZE;
                size_t frag_size = (offset + BLE_MAX_FRAGMENT_SIZE > length) ? 
                                   (length - offset) : BLE_MAX_FRAGMENT_SIZE;
                
                struct os_mbuf *om = ble_hs_mbuf_from_flat(data + offset, frag_size);
                if (om) {
                    int rc = ble_gatts_notify_custom(g_connections[i].conn_handle,
                                                      g_char_state_handle, om);
                    if (rc != 0) {
                        // Retry une fois en cas d'erreur BUSY
                        if (rc == 6) {
                            vTaskDelay(pdMS_TO_TICKS(50));
                            struct os_mbuf *om_retry = ble_hs_mbuf_from_flat(data + offset, frag_size);
                            if (om_retry) {
                                rc = ble_gatts_notify_custom(g_connections[i].conn_handle,
                                                              g_char_state_handle, om_retry);
                            }
                        }
                        
                        if (rc != 0) {
                            ESP_LOGE(TAG, "Fragment %d/%d failed for conn_handle=%d; rc=%d (%s)", 
                                     frag + 1, num_fragments, g_connections[i].conn_handle, rc,
                                     rc == 6 ? "ENOTCONN/BUSY" : "UNKNOWN");
                            result = ESP_FAIL;
                            break;
                        }
                    }
                    
                    if (frag < num_fragments - 1) {
                        vTaskDelay(pdMS_TO_TICKS(FRAGMENT_DELAY_MS));
                    }
                }
            }
        }
        
        // Délai entre les envois à différentes connexions pour éviter la congestion
        if (i < MAX_CONNECTIONS - 1 && needs_fragmentation) {
            vTaskDelay(pdMS_TO_TICKS(50));  // 50ms entre chaque app pour éviter les erreurs BUSY
        }
    }
    
    unlock_ble();
    return result;
}

esp_err_t ble_send_json(const char* json_string) {
    if (!json_string) {
        return ESP_ERR_INVALID_ARG;
    }
    return ble_send_raw((const uint8_t*)json_string, strlen(json_string));
}

// ============================================================================
// PUBLIC API - EXTERNAL DEVICE MANAGEMENT
// ============================================================================

esp_err_t ble_add_device_by_mac(const uint8_t mac_address[6], const char* device_name) {
    if (!mac_address) {
        return ESP_ERR_INVALID_ARG;
    }
    
    lock_ble();
    
    external_device_t *dev = find_free_external_device();
    if (!dev) {
        ESP_LOGE(TAG, "No free device slots");
        unlock_ble();
        return ESP_FAIL;
    }
    
    memcpy(dev->mac, mac_address, 6);
    if (device_name) {
        strncpy(dev->device_name, device_name, MAX_DEVICE_NAME_LEN - 1);
    } else {
        snprintf(dev->device_name, MAX_DEVICE_NAME_LEN, "Device_%02X%02X", 
                 mac_address[4], mac_address[5]);
    }
    dev->registered = true;
    dev->connected = false;
    dev->connecting = false;
    dev->conn_handle = 0;
    dev->received_len = 0;
    
    ESP_LOGI(TAG, "✅ Added device: %s [%02X:%02X:%02X:%02X:%02X:%02X]",
             dev->device_name,
             mac_address[0], mac_address[1], mac_address[2],
             mac_address[3], mac_address[4], mac_address[5]);
    
    unlock_ble();
    
    if (g_ble_initialized) {
        start_scan_for_external_devices();
    }
    
    return ESP_OK;
}

esp_err_t ble_start_external_scan(void) {
    start_scan_for_external_devices();
    return ESP_OK;
}

esp_err_t ble_stop_external_scan(void) {
    int rc = ble_gap_disc_cancel();
    if (rc == 0) {
        ESP_LOGI(TAG, "Scan stopped");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Failed to stop scan: rc=%d", rc);
        return ESP_FAIL;
    }
}

esp_err_t ble_remove_device_by_mac(const uint8_t mac_address[6]) {
    if (!mac_address) {
        return ESP_ERR_INVALID_ARG;
    }
    
    lock_ble();
    external_device_t *dev = find_external_device_by_mac(mac_address);
    if (dev) {
        dev->registered = false;
        unlock_ble();
        return ESP_OK;
    }
    unlock_ble();
    return ESP_ERR_NOT_FOUND;
}

bool ble_is_device_connected(const uint8_t mac_address[6]) {
    if (!mac_address) {
        return false;
    }
    
    lock_ble();
    external_device_t *dev = find_external_device_by_mac(mac_address);
    bool connected = dev ? dev->connected : false;
    unlock_ble();
    
    return connected;
}

esp_err_t ble_get_device_data(const uint8_t mac_address[6], uint8_t* out_buffer, size_t buf_size, size_t* out_len) {
    if (!mac_address || !out_buffer || !out_len) {
        return ESP_ERR_INVALID_ARG;
    }

    lock_ble();
    external_device_t *dev = find_external_device_by_mac(mac_address);
    if (!dev || !dev->registered) {
        unlock_ble();
        return ESP_ERR_NOT_FOUND;
    }

    size_t len = dev->received_len;
    if (len > buf_size) {
        // Inform caller how many bytes are available
        *out_len = len;
        unlock_ble();
        return ESP_ERR_NO_MEM;
    }

    if (len > 0) {
        memcpy(out_buffer, dev->received_data, len);
    }
    *out_len = len;
    unlock_ble();
    return ESP_OK;
}

esp_err_t ble_request_battery_update(const uint8_t mac_address[6]) {
    if (!mac_address) {
        return ESP_ERR_INVALID_ARG;
    }

    lock_ble();
    external_device_t *dev = find_external_device_by_mac(mac_address);
    if (!dev || !dev->registered || !dev->connected) {
        unlock_ble();
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t conn_handle = dev->conn_handle;
    
    // JBD TX handle must be set (no fallback!)
    if (dev->jbd_tx_handle == 0) {
        unlock_ble();
        ESP_LOGE(TAG, "❌ JBD TX handle not configured! Cannot send commands.");
        return ESP_ERR_INVALID_STATE;
    }
    
    unlock_ble();
    
    // Send JBD command 0x03 (hardware info)
    // Format: DD A5 03 00 FF FD 77
    // DD = start, A5 = read, 03 = hardware info, 00 = length
    // FFFD = checksum (0x10000 - 0x0003), 77 = end
    uint8_t cmd[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
    
    ESP_LOGD(TAG, "📤 Sending JBD command 0x03 (hardware info) to handle %d", dev->jbd_tx_handle);
    int rc = ble_gattc_write_flat(conn_handle, dev->jbd_tx_handle, cmd, sizeof(cmd), NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "❌ Failed to send JBD command: rc=%d", rc);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "✅ Command sent, waiting for notification on RX handle %d", dev->jbd_rx_handle);
    return ESP_OK;
}

// ============================================================================
// PUBLIC API - GENERIC EXTERNAL DEVICE OPERATIONS
// ============================================================================

/**
 * @brief Send raw data to an external device via GATT write
 */
esp_err_t ble_write_to_external_device(const uint8_t mac_address[6], 
                                      uint16_t attr_handle,
                                      const uint8_t* data, 
                                      size_t len) {
    if (!mac_address || !data || len == 0 || !g_ble_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    lock_ble();
    external_device_t *dev = find_external_device_by_mac(mac_address);
    if (!dev || !dev->connected) {
        unlock_ble();
        return ESP_ERR_NOT_FOUND;
    }
    
    uint16_t conn_handle = dev->conn_handle;
    unlock_ble();
    
    // Send GATT write
    int rc = ble_gattc_write_flat(conn_handle, attr_handle, data, len, NULL, NULL);
    
    if (rc == 0) {
        ESP_LOGI(TAG, "📤 Wrote %d bytes to device handle=%d", len, attr_handle);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to write to device: rc=%d", rc);
        return ESP_FAIL;
    }
}

esp_err_t ble_request_battery_cells(const uint8_t mac_address[6]) {
    if (!mac_address) {
        return ESP_ERR_INVALID_ARG;
    }

    lock_ble();
    external_device_t *dev = find_external_device_by_mac(mac_address);
    if (!dev || !dev->registered || !dev->connected) {
        unlock_ble();
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t conn_handle = dev->conn_handle;
    uint16_t tx_handle = dev->jbd_tx_handle;
    
    if (tx_handle == 0) {
        unlock_ble();
        ESP_LOGW(TAG, "JBD TX handle not found, cannot request cell data");
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    unlock_ble();
    
    // Send JBD command 0x04 (cell info)
    // Format: DD A5 04 00 FF FC 77
    // DD = start, A5 = read, 04 = cell info, 00 = length
    // FFFC = checksum (0x10000 - 0x0004), 77 = end
    uint8_t cmd[7] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
    
    ESP_LOGD(TAG, "📤 Sending JBD command 0x04 (cell voltages)");
    int rc = ble_gattc_write_flat(conn_handle, tx_handle, cmd, sizeof(cmd), NULL, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to send JBD cell command: rc=%d", rc);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// ============================================================================
// PROJECTOR APIs
// ============================================================================

/**
 * @brief Request projector status by reading the status characteristic (0x2A19)
 */
esp_err_t ble_request_projector_status(const uint8_t mac_address[6]) {
    if (!mac_address) return ESP_ERR_INVALID_ARG;

    lock_ble();
    external_device_t *dev = find_external_device_by_mac(mac_address);
    if (!dev || !dev->registered || !dev->connected) {
        unlock_ble();
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t conn_handle = dev->conn_handle;
    uint16_t status_handle = dev->proj_status_handle;
    uint16_t control_handle = dev->proj_control_handle;
    unlock_ble();

    if (control_handle == 0 || status_handle == 0) {
        ESP_LOGW(TAG, "Projector control or status handle not found for device (ctrl=%d status=%d)",
                 control_handle, status_handle);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Send PROJECTOR_CMD_GET_STATUS to the control characteristic (0x2A58)
    esp_err_t err = ble_send_projector_command(mac_address, PROJECTOR_CMD_GET_STATUS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send PROJECTOR_CMD_GET_STATUS");
        return err;
    }

    // Small delay to allow the device to prepare a response before reading
    vTaskDelay(pdMS_TO_TICKS(100));

    // Read the status characteristic (0x2A19) — response will be delivered to on_simple_read
    int rc = ble_gattc_read(conn_handle, status_handle, on_simple_read, dev);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start GATT read for projector status: rc=%d", rc);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "📥 Requested projector status (read handle=%d)", status_handle);
    return ESP_OK;
}

/**
 * @brief Send a projector command (single-byte enum) to control characteristic (0x2A58)
 */
esp_err_t ble_send_projector_command(const uint8_t mac_address[6], uint8_t cmd_byte) {
    if (!mac_address) return ESP_ERR_INVALID_ARG;

    lock_ble();
    external_device_t *dev = find_external_device_by_mac(mac_address);
    if (!dev || !dev->registered || !dev->connected) {
        unlock_ble();
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t conn_handle = dev->conn_handle;
    uint16_t ctrl_handle = dev->proj_control_handle;
    unlock_ble();

    if (ctrl_handle == 0) {
        ESP_LOGW(TAG, "Projector control handle not configured for device");
        return ESP_ERR_NOT_SUPPORTED;
    }

    int rc = ble_gattc_write_flat(conn_handle, ctrl_handle, &cmd_byte, 1, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send projector command: rc=%d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "📤 Sent projector command 0x%02X to handle %d", cmd_byte, ctrl_handle);
    return ESP_OK;
}

void ble_print_device_data(const uint8_t mac_address[6]) {
    lock_ble();
    
    if (mac_address) {
        external_device_t *dev = find_external_device_by_mac(mac_address);
        if (dev && dev->registered) {
            ESP_LOGI(TAG, "Device: %s [%02X:%02X:%02X:%02X:%02X:%02X]",
                     dev->device_name,
                     dev->mac[0], dev->mac[1], dev->mac[2],
                     dev->mac[3], dev->mac[4], dev->mac[5]);
            ESP_LOGI(TAG, "  Connected: %s", dev->connected ? "Yes" : "No");
            ESP_LOGI(TAG, "  Data: %zu bytes", dev->received_len);
        }
    } else {
        ESP_LOGI(TAG, "=== External Devices ===");
        for (int i = 0; i < MAX_EXTERNAL_DEVICES; i++) {
            if (g_external_devices[i].registered) {
                ESP_LOGI(TAG, "%d. %s [%02X:%02X:%02X:%02X:%02X:%02X] %s",
                         i, g_external_devices[i].device_name,
                         g_external_devices[i].mac[0], g_external_devices[i].mac[1],
                         g_external_devices[i].mac[2], g_external_devices[i].mac[3],
                         g_external_devices[i].mac[4], g_external_devices[i].mac[5],
                         g_external_devices[i].connected ? "✓" : "✗");
            }
        }
    }
    
    unlock_ble();
}

// ============================================================================
// PUBLIC API - DEBUG & MONITORING
// ============================================================================

void ble_print_status(void) {
    lock_ble();
    
    ESP_LOGI(TAG, "=== BLE Manager Status ===");
    ESP_LOGI(TAG, "Initialized: %s", g_ble_initialized ? "Yes" : "No");
    ESP_LOGI(TAG, "Connections: %d/%d", ble_get_connection_count(), MAX_CONNECTIONS);
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_connections[i].connected) {
            ESP_LOGI(TAG, "  [%d] handle=%d, MAC=%02X:%02X:%02X:%02X:%02X:%02X",
                     i, g_connections[i].conn_handle,
                     g_connections[i].mac[0], g_connections[i].mac[1],
                     g_connections[i].mac[2], g_connections[i].mac[3],
                     g_connections[i].mac[4], g_connections[i].mac[5]);
        }
    }
    
    unlock_ble();
}

esp_err_t ble_deinit(void) {
    if (!g_ble_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing...");
    
    lock_ble();
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_connections[i].connected) {
            ble_gap_terminate(g_connections[i].conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
    }
    unlock_ble();
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    nimble_port_stop();
    nimble_port_deinit();
    
    if (g_ble_mutex) {
        vSemaphoreDelete(g_ble_mutex);
        g_ble_mutex = NULL;
    }
    
    g_ble_initialized = false;
    ESP_LOGI(TAG, "Deinitialized");
    
    return ESP_OK;
}
