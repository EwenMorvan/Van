#include "ble_comm.h"
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "BLE_COMM";

// Variables globales
static ble_comm_t* g_comm = NULL;
static TaskHandle_t ble_host_task_handle = NULL;

// Callbacks globales
ble_data_received_cb_t g_data_received_cb = NULL;
ble_connection_cb_t g_connected_cb = NULL;
ble_connection_cb_t g_disconnected_cb = NULL;

// NimBLE variables
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t char_handle_tx = 0;
static uint16_t char_handle_rx = 0;
static QueueHandle_t ble_data_queue = NULL;

// UUIDs pour le service (utiliser des 16-bit pour simplicité)
#define BLE_SERVICE_UUID_16    0x1234
#define BLE_CHAR_TX_UUID_16    0x5678  // Pour envoyer (notify)
#define BLE_CHAR_RX_UUID_16    0x5679  // Pour recevoir (write)

// Message structure
typedef struct {
    char data[BLE_MAX_DATA_LEN];
    size_t len;
} ble_message_t;

// Forward declarations
static void ble_advertise(void);
static void ble_scan_and_connect(void);
static int ble_gap_event_handler(struct ble_gap_event *event, void *arg);
static int ble_scan_event_handler(struct ble_gap_event *event, void *arg);
static int ble_gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static void ble_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
static void ble_app_on_sync(void);
static void ble_app_on_reset(int reason);
static void ble_update_mtu(void);
static int ble_on_mtu_update(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t mtu, void *arg);

// GATT service definition
static const struct ble_gatt_svc_def ble_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SERVICE_UUID_16),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // TX characteristic (pour envoyer - notify)
                .uuid = BLE_UUID16_DECLARE(BLE_CHAR_TX_UUID_16),
                .access_cb = ble_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &char_handle_tx,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16),
                        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
                        .access_cb = ble_gatt_access_cb,
                    },
                    {
                        0, // End of descriptors
                    }
                },
            },
            {
                // RX characteristic (pour recevoir - write)
                .uuid = BLE_UUID16_DECLARE(BLE_CHAR_RX_UUID_16),
                .access_cb = ble_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &char_handle_rx,
            },
            {
                0, // End of characteristics
            }
        },
    },
    {
        0, // End of services
    }
};

// GATT access callback
static int ble_gatt_access_cb(uint16_t conn_handle_arg, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            ESP_LOGI(TAG, "GATT READ: attr_handle=%d", attr_handle);
            break;
            
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            ESP_LOGI(TAG, "GATT WRITE: attr_handle=%d, len=%d", attr_handle, ctxt->om->om_len);
            
            if (attr_handle == char_handle_rx) {
                // Données reçues via RX characteristic
                ble_message_t msg;
                msg.len = ctxt->om->om_len;
                if (msg.len >= BLE_MAX_DATA_LEN) {
                    msg.len = BLE_MAX_DATA_LEN - 1;
                }
                
                ble_hs_mbuf_to_flat(ctxt->om, msg.data, msg.len, NULL);
                msg.data[msg.len] = '\0';
                
                // Envoyer vers la queue
                if (ble_data_queue && xQueueSend(ble_data_queue, &msg, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "BLE data queue full");
                }
                
                // Appeler le callback de données reçues
                if (g_data_received_cb) {
                    g_data_received_cb(msg.data, msg.len);
                }
                
                ESP_LOGI(TAG, "📨 Received: %.*s", (int)msg.len, msg.data);
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

// GATT registration callback
static void ble_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    char buf[BLE_UUID_STR_LEN];
    
    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGI(TAG, "Registered service %s with handle=%d", 
                     ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
            break;
            
        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGI(TAG, "Registered characteristic %s with val_handle=%d", 
                     ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf), ctxt->chr.val_handle);
            
            // Capture the handles when they're registered
            if (ble_uuid_u16(ctxt->chr.chr_def->uuid) == BLE_CHAR_TX_UUID_16) {
                char_handle_tx = ctxt->chr.val_handle;
                ESP_LOGI(TAG, "Captured TX handle: %d", char_handle_tx);
            } else if (ble_uuid_u16(ctxt->chr.chr_def->uuid) == BLE_CHAR_RX_UUID_16) {
                char_handle_rx = ctxt->chr.val_handle;
                ESP_LOGI(TAG, "Captured RX handle: %d", char_handle_rx);
            }
            break;
            
        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGI(TAG, "Registered descriptor %s with handle=%d", 
                     ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf), ctxt->dsc.handle);
            break;
            
        default:
            break;
    }
}

// GAP event handler
static int ble_gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "🔗 BLE connected: status=%s", 
                     event->connect.status == 0 ? "OK" : "FAILED");
            if (event->connect.status == 0) {
                conn_handle = event->connect.conn_handle;
                if (g_comm) {
                    g_comm->connected = true;
                    g_comm->conn_handle = conn_handle;
                    g_comm->state = BLE_STATE_CONNECTED;
                }
                
                // Client connected to server - no service discovery needed
                // The client will use the known characteristic handles
                
                if (g_connected_cb) {
                    g_connected_cb();
                }

                // Update MTU size
                ble_update_mtu();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "💔 BLE disconnected");
            conn_handle = BLE_HS_CONN_HANDLE_NONE;
            char_handle_tx = 0;  // Reset handles
            char_handle_rx = 0;
            
            if (g_comm) {
                g_comm->connected = false;
                g_comm->state = BLE_STATE_DISCONNECTED;
            }
            if (g_disconnected_cb) {
                g_disconnected_cb();
            }
            
            // Redémarrer advertising si serveur, scanning si client
            if (g_comm) {
                if (g_comm->is_server) {
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Attendre avant de redémarrer
                    ble_advertise();
                } else {
                    vTaskDelay(pdMS_TO_TICKS(2000)); // Attendre avant de redémarrer le scan
                    ble_scan_and_connect();
                }
            }
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising complete");
            if (g_comm && g_comm->is_server) {
                ble_advertise(); // Continuer advertising
            }
            break;
            
        case BLE_GAP_EVENT_NOTIFY_RX:
            // Handle notifications from server (client side)
            ESP_LOGI(TAG, "📨 Notification received, len=%d", event->notify_rx.om->om_len);
            if (event->notify_rx.attr_handle == char_handle_tx) {
                ble_message_t msg;
                msg.len = event->notify_rx.om->om_len;
                if (msg.len >= BLE_MAX_DATA_LEN) {
                    msg.len = BLE_MAX_DATA_LEN - 1;
                }
                
                ble_hs_mbuf_to_flat(event->notify_rx.om, msg.data, msg.len, NULL);
                msg.data[msg.len] = '\0';
                
                // Envoyer vers la queue
                if (ble_data_queue && xQueueSend(ble_data_queue, &msg, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "BLE data queue full");
                }
                
                ESP_LOGI(TAG, "📨 Received notification: %.*s", (int)msg.len, msg.data);
            }
            break;

        default:
            break;
    }

    return 0;
}

// Start advertising (Server mode)
static void ble_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    if (!g_comm || !g_comm->is_server) {
        return;
    }

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t*)BLE_DEVICE_NAME_SERVER;
    fields.name_len = strlen(BLE_DEVICE_NAME_SERVER);
    fields.name_is_complete = 1;
    
    // Ajouter UUID du service
    static ble_uuid16_t service_uuid = BLE_UUID16_INIT(BLE_SERVICE_UUID_16);
    fields.uuids16 = &service_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising fields; rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                          &adv_params, ble_gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising; rc=%d", rc);
        return;
    }
    
    if (g_comm) {
        g_comm->state = BLE_STATE_ADVERTISING;
    }
    ESP_LOGI(TAG, "🔊 BLE advertising started as '%s'", BLE_DEVICE_NAME_SERVER);
}

// Scan event handler - moved outside function to avoid nested function issues
static int ble_scan_event_handler(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_params conn_params;
    
    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            // Vérifier si c'est notre serveur via le nom ET les UUIDs de service
            if (event->disc.length_data > 0) {
                bool found_target_server = false;
                
                // Method 1: Check device name using manual parsing
                char device_name[32] = {0};
                int name_len = 0;
                
                // Parse advertising data manually for device name
                const uint8_t *data = event->disc.data;
                uint8_t len = event->disc.length_data;
                
                // Look for complete local name (type 0x09) or shortened local name (type 0x08)
                for (int i = 0; i < len; ) {
                    uint8_t field_len = data[i];
                    if (field_len == 0 || i + field_len >= len) break;
                    
                    uint8_t field_type = data[i + 1];
                    if ((field_type == 0x09 || field_type == 0x08) && field_len > 1) {
                        // Found device name
                        name_len = (field_len - 1 < sizeof(device_name) - 1) ? 
                                   field_len - 1 : sizeof(device_name) - 1;
                        memcpy(device_name, &data[i + 2], name_len);
                        device_name[name_len] = '\0';
                        
                        if (strncmp(device_name, "VanManagement", 13) == 0) {
                            ESP_LOGI(TAG, "🎯 Found server by name: '%s'", device_name);
                            found_target_server = true;
                        }
                        break;
                    }
                    i += field_len + 1;
                }
                
                // Method 2: Check for ESP32 Communication service UUID (0x1234)
                if (!found_target_server) {
                    // Look for 16-bit service UUIDs (type 0x02 or 0x03)
                    for (int i = 0; i < len; ) {
                        uint8_t field_len = data[i];
                        if (field_len == 0 || i + field_len >= len) break;
                        
                        uint8_t field_type = data[i + 1];
                        if ((field_type == 0x02 || field_type == 0x03) && field_len >= 3) {
                            // Found 16-bit service UUIDs
                            for (int j = 2; j < field_len; j += 2) {
                                if (i + j + 1 < len) {
                                    uint16_t uuid = data[i + j] | (data[i + j + 1] << 8);
                                    ESP_LOGI(TAG, "Found service UUID: 0x%04X", uuid);
                                    if (uuid == BLE_SERVICE_UUID_16) { // 0x1234
                                        ESP_LOGI(TAG, "🎯 Found ESP32 Communication service!");
                                        found_target_server = true;
                                        break;
                                    }
                                }
                            }
                            if (found_target_server) break;
                        }
                        i += field_len + 1;
                    }
                }
                
                // Method 3: Fallback - raw data search for 0x1234 (little endian: 0x34 0x12)
                if (!found_target_server) {
                    const uint8_t *data = event->disc.data;
                    uint8_t len = event->disc.length_data;
                    
                    for (int i = 0; i < len - 3; i++) {
                        // Look for: [length] [0x03] [0x34] [0x12] (Complete 16-bit service UUIDs)
                        if (data[i] >= 3 && data[i+1] == 0x03 && 
                            data[i+2] == 0x34 && data[i+3] == 0x12) {
                            ESP_LOGI(TAG, "🎯 Found ESP32 service via raw data search!");
                            found_target_server = true;
                            break;
                        }
                        // Also check for: [length] [0x02] [0x34] [0x12] (Incomplete 16-bit service UUIDs)
                        if (data[i] >= 3 && data[i+1] == 0x02 && 
                            data[i+2] == 0x34 && data[i+3] == 0x12) {
                            ESP_LOGI(TAG, "🎯 Found ESP32 service via raw data search (incomplete)!");
                            found_target_server = true;
                            break;
                        }
                    }
                }
                
                if (found_target_server) {
                    ESP_LOGI(TAG, "🎯 Connecting to server...");
                    
                    // Arrêter le scan
                    ble_gap_disc_cancel();
                    
                    // Se connecter
                    memset(&conn_params, 0, sizeof(conn_params));
                    conn_params.scan_itvl = 0x0010;
                    conn_params.scan_window = 0x0010;
                    conn_params.itvl_min = 0x0006; // 7.5ms
                    conn_params.itvl_max = 0x000C; // 15ms
                    conn_params.latency = 0;
                    conn_params.supervision_timeout = 0x0100; // 2.56s
                    conn_params.min_ce_len = 0x0000;
                    conn_params.max_ce_len = 0x0000;
                    
                    int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr, 
                                           30000, &conn_params, ble_gap_event_handler, NULL);
                    if (rc != 0) {
                        ESP_LOGE(TAG, "Failed to connect; rc=%d", rc);
                    } else {
                        if (g_comm) {
                            g_comm->state = BLE_STATE_CONNECTING;
                        }
                    }
                    return 0;
                } else {
                    // Log pour debug
                    ESP_LOGI(TAG, "⚪ Device: %s (not our server)", 
                             name_len > 0 ? device_name : "Unknown");
                }
            }
            break;
            
        case BLE_GAP_EVENT_DISC_COMPLETE:
            ESP_LOGI(TAG, "🔍 Scan complete, retrying...");
            // Redémarrer le scan après un délai
            vTaskDelay(pdMS_TO_TICKS(2000));
            ble_scan_and_connect();
            break;
            
        default:
            break;
    }
    return 0;
}

// Scan for server (Client mode)
static void ble_scan_and_connect(void) {
    struct ble_gap_disc_params disc_params;
    int rc;

    if (!g_comm || g_comm->is_server) {
        return;
    }

    // Rechercher par nom de device - pour le mode client, on cherche le serveur
    ESP_LOGI(TAG, "🔍 Scanning for BLE server...");
    
    memset(&disc_params, 0, sizeof(disc_params));
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;
    
    rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 10000, &disc_params, ble_scan_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start scanning; rc=%d", rc);
        return;
    }

    if (g_comm) {
        g_comm->state = BLE_STATE_SCANNING;
    }
}

// Called when NimBLE host and controller sync
static void ble_app_on_sync(void) {
    int rc;
    
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to ensure address; rc=%d", rc);
        return;
    }
    
    ESP_LOGI(TAG, "✅ BLE host synced");
    
    // Set device name
    const char* device_name = (g_comm && g_comm->is_server) ? 
                             BLE_DEVICE_NAME_SERVER : BLE_DEVICE_NAME_CLIENT;
    rc = ble_svc_gap_device_name_set(device_name);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name; rc=%d", rc);
        return;
    }
    
    // Add GATT services if server
    if (g_comm && g_comm->is_server) {
        ESP_LOGI(TAG, "Starting GATT services initialization...");
        ESP_LOGI(TAG, "Service UUID: 0x%04X", BLE_SERVICE_UUID_16);
        ESP_LOGI(TAG, "TX UUID: 0x%04X", BLE_CHAR_TX_UUID_16); 
        ESP_LOGI(TAG, "RX UUID: 0x%04X", BLE_CHAR_RX_UUID_16);
        
        rc = ble_gatts_count_cfg(ble_gatt_svcs);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to count GATT config; rc=%d", rc);
            return;
        }
        ESP_LOGI(TAG, "GATT configuration counted successfully");
        
        rc = ble_gatts_add_svcs(ble_gatt_svcs);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to add GATT services; rc=%d", rc);
            return;
        }
        ESP_LOGI(TAG, "GATT services added successfully");
        
        rc = ble_gatts_start();
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to start GATT services; rc=%d", rc);
            return;
        }
        ESP_LOGI(TAG, "GATT services started successfully");
        
        // Wait for NimBLE to assign handles
        vTaskDelay(pdMS_TO_TICKS(200));
        
        ESP_LOGI(TAG, "Final handles: TX=%d, RX=%d", char_handle_tx, char_handle_rx);
        
        // Start advertising
        ble_advertise();
    } else {
        // Start scanning (client mode)
        ble_scan_and_connect();
    }
}

// Called when NimBLE host resets
static void ble_app_on_reset(int reason) {
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

// NimBLE host task
static void ble_host_task_func(void *param) {
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run(); // This function should never return
}

// Increase MTU size to support larger messages
static int ble_on_mtu_update(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t mtu, void *arg) {
    if (error && error->status != 0) {
        ESP_LOGW(TAG, "MTU update failed: conn_handle=%d error=%d", conn_handle, error->status);
        return error->status;
    }
    ESP_LOGI(TAG, "MTU updated: conn_handle=%d mtu=%d", conn_handle, mtu);
    return 0;
}

// Update MTU during BLE initialization
static void ble_update_mtu(void) {
    int rc = ble_att_set_preferred_mtu(247); // Set preferred MTU to maximum (247 bytes)
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to set preferred MTU: rc=%d", rc);
    }

    rc = ble_gattc_exchange_mtu(conn_handle, ble_on_mtu_update, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to initiate MTU exchange: rc=%d", rc);
    }
}

// Fonctions utilitaires
const char* ble_comm_get_state_string(ble_state_t state) {
    switch (state) {
        case BLE_STATE_IDLE: return "IDLE";
        case BLE_STATE_ADVERTISING: return "ADVERTISING";
        case BLE_STATE_SCANNING: return "SCANNING";
        case BLE_STATE_CONNECTING: return "CONNECTING";
        case BLE_STATE_CONNECTED: return "CONNECTED";
        case BLE_STATE_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

bool ble_comm_is_connected(ble_comm_t *comm) {
    return (comm && comm->connected);
}

// Initialisation
esp_err_t ble_comm_init(ble_comm_t *comm, bool is_server) {
    if (!comm) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(comm, 0, sizeof(ble_comm_t));
    comm->is_server = is_server;
    comm->state = BLE_STATE_IDLE;
    comm->connected = false;
    
    comm->mutex = xSemaphoreCreateMutex();
    comm->data_ready_sem = xSemaphoreCreateBinary();
    
    if (!comm->mutex || !comm->data_ready_sem) {
        ESP_LOGE(TAG, "Failed to create semaphores");
        return ESP_ERR_NO_MEM;
    }
    
    // Create data queue
    ble_data_queue = xQueueCreate(10, sizeof(ble_message_t));
    if (!ble_data_queue) {
        ESP_LOGE(TAG, "Failed to create BLE data queue");
        return ESP_ERR_NO_MEM;
    }
    
    g_comm = comm;
    
    // Initialize NimBLE
    nimble_port_init();
    
    // Configure the host
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.reset_cb = ble_app_on_reset;
    ble_hs_cfg.gatts_register_cb = ble_gatt_register_cb;
    
    ESP_LOGI(TAG, "✅ BLE comm initialized as %s", is_server ? "SERVER" : "CLIENT");
    return ESP_OK;
}

// Démarrer la communication
esp_err_t ble_comm_start(ble_comm_t *comm) {
    if (!comm) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task_func);
    
    ESP_LOGI(TAG, "🚀 BLE started as %s", comm->is_server ? "SERVER" : "CLIENT");
    return ESP_OK;
}

// Envoyer des données
esp_err_t ble_comm_send(ble_comm_t *comm, const char *data, size_t len) {
    if (!comm || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!comm->connected || conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG, "BLE not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (len > BLE_MAX_DATA_LEN) {
        ESP_LOGW(TAG, "Data too long, truncating");
        len = BLE_MAX_DATA_LEN;
    }
    
    int rc;
    
    if (comm->is_server) {
        // Server mode: send notification via TX characteristic
        if (char_handle_tx == 0) {
            ESP_LOGE(TAG, "TX characteristic handle not ready");
            return ESP_ERR_INVALID_STATE;
        }
        
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
        if (om == NULL) {
            ESP_LOGE(TAG, "Failed to allocate mbuf");
            return ESP_ERR_NO_MEM;
        }
        
        rc = ble_gatts_notify_custom(conn_handle, char_handle_tx, om);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to send notification; rc=%d", rc);
            return ESP_FAIL;
        }
    } else {
        // Client mode: write to server's ESP32 RX characteristic
        // We use hard-coded handle 14 (from server logs: "Captured ESP32 RX handle: 14")
        uint16_t server_rx_handle = 14;
        
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
        if (om == NULL) {
            ESP_LOGE(TAG, "Failed to allocate mbuf for client write");
            return ESP_ERR_NO_MEM;
        }
        
        rc = ble_gattc_write_flat(conn_handle, server_rx_handle, data, len, NULL, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to write to server; rc=%d", rc);
            return ESP_FAIL;
        }
    }
    
    ESP_LOGI(TAG, "📤 Sent (%s): %.*s", comm->is_server ? "notify" : "write", (int)len, data);
    return ESP_OK;
}

// Recevoir des données
esp_err_t ble_comm_recv(ble_comm_t *comm, char *buffer, size_t buffer_len, size_t *recv_len) {
    if (!comm || !buffer || !recv_len) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *recv_len = 0;
    
    ble_message_t msg;
    if (xQueueReceive(ble_data_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
        size_t copy_len = (msg.len < buffer_len - 1) ? msg.len : buffer_len - 1;
        memcpy(buffer, msg.data, copy_len);
        buffer[copy_len] = '\0';
        *recv_len = copy_len;
        
        // Signal data ready semaphore
        xSemaphoreGive(comm->data_ready_sem);
        
        // Call callback if set
        if (g_data_received_cb) {
            g_data_received_cb(buffer, *recv_len);
        }
        
        return ESP_OK;
    }
    
    return ESP_ERR_NOT_FOUND;
}

// Arrêter la communication
esp_err_t ble_comm_stop(ble_comm_t *comm) {
    if (!comm) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (comm->is_server) {
        ble_gap_adv_stop();
    } else {
        ble_gap_disc_cancel();
    }
    
    comm->state = BLE_STATE_IDLE;
    return ESP_OK;
}

// Désinitialisation
void ble_comm_deinit(ble_comm_t *comm) {
    if (!comm) return;
    
    ble_comm_stop(comm);
    
    if (comm->mutex) {
        vSemaphoreDelete(comm->mutex);
        comm->mutex = NULL;
    }
    
    if (comm->data_ready_sem) {
        vSemaphoreDelete(comm->data_ready_sem);
        comm->data_ready_sem = NULL;
    }
    
    if (ble_data_queue) {
        vQueueDelete(ble_data_queue);
        ble_data_queue = NULL;
    }
    
    if (ble_host_task_handle) {
        vTaskDelete(ble_host_task_handle);
        ble_host_task_handle = NULL;
    }
    
    nimble_port_stop();
    nimble_port_deinit();
    
    g_comm = NULL;
    ESP_LOGI(TAG, "BLE comm deinitialized");
}

/**
 * @brief Set BLE callbacks
 */
void ble_comm_set_callbacks(ble_connection_cb_t connected_cb, ble_connection_cb_t disconnected_cb, ble_data_received_cb_t data_cb) {
    g_connected_cb = connected_cb;
    g_disconnected_cb = disconnected_cb;
    g_data_received_cb = data_cb;
    ESP_LOGI(TAG, "BLE callbacks set");
}