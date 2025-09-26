// #include "ble_manager_nimble.h"
// #include "communication_protocol.h"
// #include "esp_log.h"
// #include "nvs_flash.h"
// #include "nimble/nimble_port.h"
// #include "nimble/nimble_port_freertos.h"
// #include "host/ble_hs.h"
// #include "host/ble_uuid.h"
// #include "host/ble_gatt.h"
// #include "host/util/util.h"
// #include "services/gap/ble_svc_gap.h"
// #include "services/gatt/ble_svc_gatt.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
// #include <string.h>

// static const char *TAG = "BLE_MGR";
// static TaskHandle_t ble_task_handle;

// // Forward declarations
// static void van_advertise(void);
// static esp_err_t ble_send_data(const char* data, size_t length);
// static void van_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

// // BLE service and characteristic UUIDs - Using 16-bit UUIDs for simplicity
// #define VAN_SERVICE_UUID_16         0xAAA0
// #define VAN_CHAR_COMMAND_UUID_16    0xAAA1  
// #define VAN_CHAR_STATE_UUID_16      0xAAA2

// #define MAX_BLE_MESSAGE_SIZE    512

// static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
// static uint16_t char_command_handle;
// static uint16_t char_state_handle;
// static bool ble_connected = false;

// // Message queue for BLE commands
// static QueueHandle_t ble_command_queue;

// // GATT registration callback
// static void van_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
//     char buf[BLE_UUID_STR_LEN];
    
//     switch (ctxt->op) {
//         case BLE_GATT_REGISTER_OP_SVC:
//             ESP_LOGI(TAG, "Registered service %s with handle=%d", 
//                      ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
//             break;
            
//         case BLE_GATT_REGISTER_OP_CHR:
//             ESP_LOGI(TAG, "Registered characteristic %s with val_handle=%d", 
//                      ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf), ctxt->chr.val_handle);
            
//             // Capture the handles when they're registered
//             if (ble_uuid_u16(ctxt->chr.chr_def->uuid) == VAN_CHAR_COMMAND_UUID_16) {
//                 char_command_handle = ctxt->chr.val_handle;
//                 ESP_LOGI(TAG, "Captured command handle: %d", char_command_handle);
//             } else if (ble_uuid_u16(ctxt->chr.chr_def->uuid) == VAN_CHAR_STATE_UUID_16) {
//                 char_state_handle = ctxt->chr.val_handle;
//                 ESP_LOGI(TAG, "Captured state handle: %d", char_state_handle);
//             }
//             break;
            
//         case BLE_GATT_REGISTER_OP_DSC:
//             ESP_LOGI(TAG, "Registered descriptor %s with handle=%d", 
//                      ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf), ctxt->dsc.handle);
//             break;
            
//         default:
//             break;
//     }
// }

// // GATT service access callback
// static int van_gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
//                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
//     ESP_LOGI(TAG, "GATT access callback called: op=%d, conn_handle=%d, attr_handle=%d", 
//              ctxt->op, conn_handle, attr_handle);
    
//     switch (ctxt->op) {
//         case BLE_GATT_ACCESS_OP_READ_CHR:
//             ESP_LOGI(TAG, "GATT READ characteristic: attr_handle=%d", attr_handle);
//             break;
            
//         case BLE_GATT_ACCESS_OP_WRITE_CHR:
//             ESP_LOGI(TAG, "GATT WRITE characteristic: attr_handle=%d, len=%d", attr_handle, ctxt->om->om_len);
            
//             // Handle command characteristic write (using 16-bit UUID comparison)
//             if (attr_handle == char_command_handle) {
//                 char cmd_buffer[MAX_BLE_MESSAGE_SIZE];
//                 uint16_t data_len = ctxt->om->om_len;
                
//                 if (data_len >= MAX_BLE_MESSAGE_SIZE) {
//                     data_len = MAX_BLE_MESSAGE_SIZE - 1;
//                 }
                
//                 ble_hs_mbuf_to_flat(ctxt->om, cmd_buffer, data_len, NULL);
//                 cmd_buffer[data_len] = '\0';
                
//                 // Send to command queue
//                 if (xQueueSend(ble_command_queue, cmd_buffer, 0) != pdTRUE) {
//                     ESP_LOGW(TAG, "BLE command queue full");
//                 }
//             }
//             break;
            
//         default:
//             break;
//     }
    
//     return 0;
// }

// // GATT service definition
// static const struct ble_gatt_svc_def van_gatt_svcs[] = {
//     {
//         .type = BLE_GATT_SVC_TYPE_PRIMARY,
//         .uuid = BLE_UUID16_DECLARE(VAN_SERVICE_UUID_16),
//         .characteristics = (struct ble_gatt_chr_def[]) {
//             {
//                 // Command characteristic (write)
//                 .uuid = BLE_UUID16_DECLARE(VAN_CHAR_COMMAND_UUID_16),
//                 .access_cb = van_gatt_access_cb,
//                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
//                 .val_handle = &char_command_handle,
//             },
//             {
//                 // State characteristic (notify)
//                 .uuid = BLE_UUID16_DECLARE(VAN_CHAR_STATE_UUID_16),
//                 .access_cb = van_gatt_access_cb,
//                 .flags = BLE_GATT_CHR_F_NOTIFY,
//                 .val_handle = &char_state_handle,
//                 .descriptors = (struct ble_gatt_dsc_def[]) {
//                     {
//                         .uuid = BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16),
//                         .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
//                         .access_cb = van_gatt_access_cb,
//                     },
//                     {
//                         0, // End of descriptors
//                     }
//                 },
//             },
//             {
//                 0, // No more characteristics
//             }
//         },
//     },
//     {
//         0, // No more services
//     }
// };

// // GAP event handler
// static int van_gap_event(struct ble_gap_event *event, void *arg) {
//     switch (event->type) {
//         case BLE_GAP_EVENT_CONNECT:
//             ESP_LOGI(TAG, "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK" : "FAILED");
//             if (event->connect.status == 0) {
//                 conn_handle = event->connect.conn_handle;
//                 ble_connected = true;
//                 comm_protocol_set_connected(COMM_INTERFACE_BLE, true);
//             }
//             break;

//         case BLE_GAP_EVENT_DISCONNECT:
//             ESP_LOGI(TAG, "BLE GAP EVENT DISCONNECT");
//             conn_handle = BLE_HS_CONN_HANDLE_NONE;
//             ble_connected = false;
//             comm_protocol_set_connected(COMM_INTERFACE_BLE, false);
//             van_advertise(); // Start advertising again
//             break;

//         case BLE_GAP_EVENT_ADV_COMPLETE:
//             ESP_LOGI(TAG, "BLE GAP EVENT ADV COMPLETE");
//             van_advertise();
//             break;

//         case BLE_GAP_EVENT_SUBSCRIBE:
//             ESP_LOGI(TAG, "BLE GAP EVENT SUBSCRIBE - Client %s notifications", 
//                      event->subscribe.cur_notify ? "enabled" : "disabled");
//             if (event->subscribe.cur_notify) {
//                 ESP_LOGI(TAG, "Client subscribed to notifications on char_handle=%d", event->subscribe.attr_handle);
//             }
//             break;

//         default:
//             break;
//     }

//     return 0;
// }

// // Start advertising
// static void van_advertise(void) {
//     struct ble_gap_adv_params adv_params;
//     struct ble_hs_adv_fields fields;
//     int rc;

//     memset(&fields, 0, sizeof(fields));
//     fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
//     fields.tx_pwr_lvl_is_present = 1;
//     fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
//     fields.name = (uint8_t*)"VanManagement";
//     fields.name_len = strlen("VanManagement");
//     fields.name_is_complete = 1;
    
//     // Add our service UUID to advertising to make it more visible
//     static ble_uuid16_t service_uuid = BLE_UUID16_INIT(VAN_SERVICE_UUID_16);
//     fields.uuids16 = &service_uuid;
//     fields.num_uuids16 = 1;
//     fields.uuids16_is_complete = 1;
    
//     rc = ble_gap_adv_set_fields(&fields);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "Failed to set advertising fields; rc=%d", rc);
//         return;
//     }

//     memset(&adv_params, 0, sizeof(adv_params));
//     adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
//     adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
//     rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
//                           &adv_params, van_gap_event, NULL);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "Failed to start advertising; rc=%d", rc);
//         return;
//     }
    
//     ESP_LOGI(TAG, "BLE advertising started");
// }

// // Called when NimBLE host and controller sync
// static void ble_app_on_sync(void) {
//     int rc;
    
//     rc = ble_hs_util_ensure_addr(0);
//     assert(rc == 0);
    
//     ESP_LOGI(TAG, "BLE host synced, initializing GATT services");
    
//     // Set device name first
//     rc = ble_svc_gap_device_name_set("VanManagement");
//     if (rc != 0) {
//         ESP_LOGE(TAG, "Failed to set device name; rc=%d", rc);
//         return;
//     }
//     ESP_LOGI(TAG, "Device name set successfully");
    
//     // Initialize GATT services
//     ESP_LOGI(TAG, "Starting GATT services initialization...");
//     ESP_LOGI(TAG, "Service UUID: 0x%04X", VAN_SERVICE_UUID_16);
//     ESP_LOGI(TAG, "Command UUID: 0x%04X", VAN_CHAR_COMMAND_UUID_16); 
//     ESP_LOGI(TAG, "State UUID: 0x%04X", VAN_CHAR_STATE_UUID_16);
    
//     rc = ble_gatts_count_cfg(van_gatt_svcs);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "Failed to count GATT configuration; rc=%d", rc);
//         return;
//     }
//     ESP_LOGI(TAG, "GATT configuration counted successfully");
    
//     rc = ble_gatts_add_svcs(van_gatt_svcs);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "Failed to add GATT services; rc=%d", rc);
//         return;
//     }
//     ESP_LOGI(TAG, "GATT services added successfully");
    
//     // Start GATT services
//     rc = ble_gatts_start();
//     if (rc != 0) {
//         ESP_LOGE(TAG, "Failed to start GATT services; rc=%d", rc);
//         return;
//     }
//     ESP_LOGI(TAG, "GATT services started successfully");
    
//     // Wait for NimBLE to assign handles
//     vTaskDelay(pdMS_TO_TICKS(200));
    
//     ESP_LOGI(TAG, "Checking assigned handles: command=%d, state=%d", 
//              char_command_handle, char_state_handle);
             
//     if (char_command_handle == 0 || char_state_handle == 0) {
//         ESP_LOGW(TAG, "Handles not assigned yet, they will be captured later");
//     } else {
//         ESP_LOGI(TAG, "GATT handles properly assigned - services should be visible!");
//         ESP_LOGI(TAG, "Final handles: command=%d, state=%d", char_command_handle, char_state_handle);
//     }
             
//     ESP_LOGI(TAG, "GATT services initialization complete");

//     van_advertise();
// }

// // Called when NimBLE host resets
// static void ble_app_on_reset(int reason) {
//     ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
// }

// // NimBLE host task function
// static void ble_host_task(void *param) {
//     ESP_LOGI(TAG, "BLE host task started");
//     nimble_port_run(); // This function should never return
// }

// // BLE task
// static void ble_task(void *param) {
//     char cmd_buffer[MAX_BLE_MESSAGE_SIZE];
//     static bool interface_registered = false;
    
//     while (1) {
//         // Check if we need to register the interface once handles are ready
//         if (!interface_registered && char_state_handle != 0) {
//             esp_err_t ret = comm_protocol_register_interface(COMM_INTERFACE_BLE, ble_send_data);
//             if (ret == ESP_OK) {
//                 ESP_LOGI(TAG, "BLE interface registered with communication protocol (handles ready)");
//                 interface_registered = true;
//             } else {
//                 ESP_LOGE(TAG, "Failed to register BLE interface: %s", esp_err_to_name(ret));
//             }
//         }
        
//         // Wait for commands from BLE
//         if (xQueueReceive(ble_command_queue, cmd_buffer, pdMS_TO_TICKS(100)) == pdTRUE) {
//             ESP_LOGC(TAG, "Received BLE command: %s", cmd_buffer);
            
//             // Process the received command through the communication protocol
//             comm_protocol_process_received_data(COMM_INTERFACE_BLE, cmd_buffer, strlen(cmd_buffer));
//         }
        
//         // Periodic state broadcasting is handled by the communication protocol task
//         vTaskDelay(pdMS_TO_TICKS(100));
//     }
// }

// esp_err_t ble_manager_init(void) {
//     esp_err_t ret;
    
//     // Create command queue
//     ble_command_queue = xQueueCreate(10, MAX_BLE_MESSAGE_SIZE);
//     if (ble_command_queue == NULL) {
//         ESP_LOGE(TAG, "Failed to create BLE command queue");
//         return ESP_FAIL;
//     }
    
//     // Initialize NVS for NimBLE
//     ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);
    
//     // Initialize NimBLE
//     nimble_port_init();
    
//     // Configure the host
//     ble_hs_cfg.sync_cb = ble_app_on_sync;
//     ble_hs_cfg.reset_cb = ble_app_on_reset;
    
//     // Start NimBLE host task
//     nimble_port_freertos_init(ble_host_task);
    
//     // Create BLE manager task
//     if (xTaskCreate(ble_task, "ble_task", 8192, NULL, 5, &ble_task_handle) != pdTRUE) {
//         ESP_LOGE(TAG, "Failed to create BLE task");
//         return ESP_FAIL;
//     }
    
//     ESP_LOGI(TAG, "BLE Manager initialized successfully");
//     return ESP_OK;
// }

// static esp_err_t ble_send_data(const char* data, size_t length) {
//     if (!ble_connected || conn_handle == BLE_HS_CONN_HANDLE_NONE) {
//         ESP_LOGW(TAG, "BLE not connected or invalid connection handle");
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     if (char_state_handle == 0) {
//         ESP_LOGE(TAG, "State characteristic handle is 0 - GATT service not properly initialized");
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     ESP_LOGI(TAG, "Sending notification: conn_handle=%d, char_handle=%d, data_len=%d", 
//              conn_handle, char_state_handle, length);
//     ESP_LOGI(TAG, "Data preview: %.100s%s", data, length > 100 ? "..." : "");
    
//     // BLE MTU is typically 20-244 bytes. For compatibility, use smaller chunks
//     const size_t chunk_size = 200;  // Conservative chunk size
//     size_t offset = 0;
    
//     while (offset < length) {
//         size_t current_chunk = (length - offset > chunk_size) ? chunk_size : (length - offset);
        
//         struct os_mbuf *om = ble_hs_mbuf_from_flat(data + offset, current_chunk);
//         if (om == NULL) {
//             ESP_LOGE(TAG, "Failed to allocate mbuf for notification chunk");
//             return ESP_ERR_NO_MEM;
//         }
        
//         int rc = ble_gatts_notify_custom(conn_handle, char_state_handle, om);
//         if (rc != 0) {
//             ESP_LOGE(TAG, "Failed to send notification chunk %d; rc=%d", (int)(offset/chunk_size), rc);
//             return ESP_FAIL;
//         }
        
//         offset += current_chunk;
        
//         // Small delay between chunks to avoid overwhelming the BLE stack
//         if (offset < length) {
//             vTaskDelay(pdMS_TO_TICKS(10));
//         }
        
//         ESP_LOGD(TAG, "Sent chunk %d/%d (%d bytes)", 
//                  (int)(offset/chunk_size), (int)((length + chunk_size - 1)/chunk_size), (int)current_chunk);
//     }
    
//     ESP_LOGI(TAG, "All notification chunks sent successfully (%d total chunks)", 
//              (int)((length + chunk_size - 1)/chunk_size));
//     return ESP_OK;
// }

// esp_err_t ble_manager_send_state(const char* state_json) {
//     return ble_send_data(state_json, strlen(state_json));
// }

// bool ble_manager_is_connected(void) {
//     return ble_connected;
// }

// esp_err_t ble_manager_deinit(void) {
//     if (ble_task_handle) {
//         vTaskDelete(ble_task_handle);
//         ble_task_handle = NULL;
//     }
    
//     if (ble_command_queue) {
//         vQueueDelete(ble_command_queue);
//         ble_command_queue = NULL;
//     }
    
//     nimble_port_stop();
//     nimble_port_deinit();
    
//     ESP_LOGI(TAG, "BLE Manager deinitialized");
//     return ESP_OK;
// }


#include "ble_manager_nimble.h"
#include "communication_protocol.h"
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
#include <string.h>

static const char *TAG = "BLE_MGR";
static TaskHandle_t ble_task_handle;

// Forward declarations
static void van_advertise(void);
static esp_err_t ble_send_data(const char* data, size_t length);
static void van_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

// BLE service and characteristic UUIDs - Using 16-bit UUIDs for simplicity
// Van Management Service (for smartphones)
#define VAN_SERVICE_UUID_16         0xAAA0
#define VAN_CHAR_COMMAND_UUID_16    0xAAA1  
#define VAN_CHAR_STATE_UUID_16      0xAAA2

// ESP32 Communication Service (for ESP32 client)
#define ESP32_COMM_SERVICE_UUID_16  0x1234
#define ESP32_CHAR_TX_UUID_16       0x5678  // Pour envoyer (notify)
#define ESP32_CHAR_RX_UUID_16       0x5679  // Pour recevoir (write)

#define MAX_BLE_MESSAGE_SIZE    512
#define MAX_CONNECTIONS         4   // Support multiple connections

// Connection management
typedef struct {
    uint16_t conn_handle;
    bool is_smartphone;  // true = smartphone, false = ESP32 client
    bool connected;
} ble_connection_t;

static ble_connection_t connections[MAX_CONNECTIONS];
static uint16_t active_connections = 0;

// Van Management Service handles
static uint16_t char_command_handle;
static uint16_t char_state_handle;

// ESP32 Communication Service handles  
static uint16_t esp32_char_tx_handle;
static uint16_t esp32_char_rx_handle;

// Backward compatibility
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool ble_connected = false;  // At least one connection

// Message queue for BLE commands
static QueueHandle_t ble_command_queue;

// Connection management helper functions
static int find_free_connection_slot(void) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!connections[i].connected) {
            return i;
        }
    }
    return -1;
}

static int find_connection_by_handle(uint16_t handle) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].connected && connections[i].conn_handle == handle) {
            return i;
        }
    }
    return -1;
}

// GATT registration callback
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
            
            // Capture the handles when they're registered
            uint16_t uuid = ble_uuid_u16(ctxt->chr.chr_def->uuid);
            if (uuid == VAN_CHAR_COMMAND_UUID_16) {
                char_command_handle = ctxt->chr.val_handle;
                ESP_LOGI(TAG, "Captured van command handle: %d", char_command_handle);
            } else if (uuid == VAN_CHAR_STATE_UUID_16) {
                char_state_handle = ctxt->chr.val_handle;
                ESP_LOGI(TAG, "Captured van state handle: %d", char_state_handle);
            } else if (uuid == ESP32_CHAR_TX_UUID_16) {
                esp32_char_tx_handle = ctxt->chr.val_handle;
                ESP_LOGI(TAG, "Captured ESP32 TX handle: %d", esp32_char_tx_handle);
            } else if (uuid == ESP32_CHAR_RX_UUID_16) {
                esp32_char_rx_handle = ctxt->chr.val_handle;
                ESP_LOGI(TAG, "Captured ESP32 RX handle: %d", esp32_char_rx_handle);
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

// GATT service access callback
static int van_gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(TAG, "GATT access callback called: op=%d, conn_handle=%d, attr_handle=%d", 
             ctxt->op, conn_handle, attr_handle);
    
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            ESP_LOGI(TAG, "GATT READ characteristic: attr_handle=%d", attr_handle);
            break;
            
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            ESP_LOGI(TAG, "GATT WRITE characteristic: attr_handle=%d, len=%d", attr_handle, ctxt->om->om_len);
            
            char cmd_buffer[MAX_BLE_MESSAGE_SIZE];
            uint16_t data_len = ctxt->om->om_len;
            
            if (data_len >= MAX_BLE_MESSAGE_SIZE) {
                data_len = MAX_BLE_MESSAGE_SIZE - 1;
            }
            
            ble_hs_mbuf_to_flat(ctxt->om, cmd_buffer, data_len, NULL);
            cmd_buffer[data_len] = '\0';
            
            // Handle different characteristic writes and identify client type
            if (attr_handle == char_command_handle) {
                // Smartphone command - mark connection as smartphone
                int slot = find_connection_by_handle(conn_handle);
                if (slot >= 0 && !connections[slot].is_smartphone) {
                    connections[slot].is_smartphone = true;
                    ESP_LOGI(TAG, "📱 Identified connection %d as smartphone", slot);
                }
                
                ESP_LOGI(TAG, "📱 Smartphone command: %s", cmd_buffer);
                // Send to command queue
                if (xQueueSend(ble_command_queue, cmd_buffer, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "BLE command queue full");
                }
            } else if (attr_handle == esp32_char_rx_handle) {
                // ESP32 client message - mark connection as ESP32
                int slot = find_connection_by_handle(conn_handle);
                if (slot >= 0 && connections[slot].is_smartphone) {
                    connections[slot].is_smartphone = false;
                    ESP_LOGI(TAG, "🔌 Identified connection %d as ESP32 client", slot);
                }
                
                ESP_LOGI(TAG, "🔌 ESP32 client message: %s", cmd_buffer);
                // Forward to command queue with ESP32 prefix
                char prefixed_cmd[MAX_BLE_MESSAGE_SIZE + 10];
                snprintf(prefixed_cmd, sizeof(prefixed_cmd), "ESP32:%s", cmd_buffer);
                if (xQueueSend(ble_command_queue, prefixed_cmd, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "BLE command queue full");
                }
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

// GATT service definition - Two services: Van Management + ESP32 Communication
static const struct ble_gatt_svc_def van_gatt_svcs[] = {
    {
        // Van Management Service (for smartphones)
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(VAN_SERVICE_UUID_16),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Command characteristic (write)
                .uuid = BLE_UUID16_DECLARE(VAN_CHAR_COMMAND_UUID_16),
                .access_cb = van_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &char_command_handle,
            },
            {
                // State characteristic (notify)
                .uuid = BLE_UUID16_DECLARE(VAN_CHAR_STATE_UUID_16),
                .access_cb = van_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &char_state_handle,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16),
                        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
                        .access_cb = van_gatt_access_cb,
                    },
                    {
                        0, // End of descriptors
                    }
                },
            },
            {
                0, // No more characteristics
            }
        },
    },
    {
        // ESP32 Communication Service (for ESP32 client)
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(ESP32_COMM_SERVICE_UUID_16),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // TX characteristic (pour envoyer - notify)
                .uuid = BLE_UUID16_DECLARE(ESP32_CHAR_TX_UUID_16),
                .access_cb = van_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &esp32_char_tx_handle,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16),
                        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
                        .access_cb = van_gatt_access_cb,
                    },
                    {
                        0, // End of descriptors
                    }
                },
            },
            {
                // RX characteristic (pour recevoir - write)
                .uuid = BLE_UUID16_DECLARE(ESP32_CHAR_RX_UUID_16),
                .access_cb = van_gatt_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &esp32_char_rx_handle,
            },
            {
                0, // End of characteristics
            }
        },
    },
    {
        0, // No more services
    }
};

// GAP event handler
static int van_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK" : "FAILED");
            if (event->connect.status == 0) {
                int slot = find_free_connection_slot();
                if (slot >= 0) {
                    connections[slot].conn_handle = event->connect.conn_handle;
                    connections[slot].connected = true;
                    // We'll determine if it's smartphone vs ESP32 based on service usage
                    connections[slot].is_smartphone = true; // Default to smartphone
                    active_connections++;
                    
                    // Keep backward compatibility
                    if (!ble_connected) {
                        conn_handle = event->connect.conn_handle;
                        ble_connected = true;
                        comm_protocol_set_connected(COMM_INTERFACE_BLE, true);
                    }
                    
                    ESP_LOGI(TAG, "📱 New connection in slot %d (handle=%d), total=%d", 
                             slot, event->connect.conn_handle, active_connections);
                } else {
                    ESP_LOGW(TAG, "⚠️  No free connection slots, disconnecting client");
                    ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                }
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE GAP EVENT DISCONNECT (handle=%d)", event->disconnect.conn.conn_handle);
            int slot = find_connection_by_handle(event->disconnect.conn.conn_handle);
            if (slot >= 0) {
                ESP_LOGI(TAG, "💔 Disconnected %s in slot %d (handle=%d)", 
                         connections[slot].is_smartphone ? "smartphone" : "ESP32", 
                         slot, event->disconnect.conn.conn_handle);
                connections[slot].connected = false;
                active_connections--;
            }
            
            // Update backward compatibility variables
            if (event->disconnect.conn.conn_handle == conn_handle) {
                conn_handle = BLE_HS_CONN_HANDLE_NONE;
                // Find another connection to use as primary
                for (int i = 0; i < MAX_CONNECTIONS; i++) {
                    if (connections[i].connected) {
                        conn_handle = connections[i].conn_handle;
                        break;
                    }
                }
            }
            
            if (active_connections == 0) {
                ble_connected = false;
                comm_protocol_set_connected(COMM_INTERFACE_BLE, false);
            }
            
            van_advertise(); // Start advertising again
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "BLE GAP EVENT ADV COMPLETE");
            van_advertise();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "BLE GAP EVENT SUBSCRIBE - Client %s notifications", 
                     event->subscribe.cur_notify ? "enabled" : "disabled");
            if (event->subscribe.cur_notify) {
                ESP_LOGI(TAG, "Client subscribed to notifications on char_handle=%d", event->subscribe.attr_handle);
            }
            break;

        default:
            break;
    }

    return 0;
}

// Start advertising
static void van_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t*)"VanManagement";
    fields.name_len = strlen("VanManagement");
    fields.name_is_complete = 1;
    
    // Add both service UUIDs to advertising for visibility
    static ble_uuid16_t service_uuids[2];
    service_uuids[0] = (ble_uuid16_t)BLE_UUID16_INIT(VAN_SERVICE_UUID_16);
    service_uuids[1] = (ble_uuid16_t)BLE_UUID16_INIT(ESP32_COMM_SERVICE_UUID_16);
    fields.uuids16 = service_uuids;
    fields.num_uuids16 = 2;
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
                          &adv_params, van_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising; rc=%d", rc);
        return;
    }
    
    ESP_LOGI(TAG, "BLE advertising started");
}

// Called when NimBLE host and controller sync
static void ble_app_on_sync(void) {
    int rc;
    
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    
    ESP_LOGI(TAG, "BLE host synced, initializing GATT services");
    
    // Set device name first
    rc = ble_svc_gap_device_name_set("VanManagement");
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name; rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "Device name set successfully");
    
    // Initialize GATT services
    ESP_LOGI(TAG, "Starting GATT services initialization...");
    ESP_LOGI(TAG, "Service UUID: 0x%04X", VAN_SERVICE_UUID_16);
    ESP_LOGI(TAG, "Command UUID: 0x%04X", VAN_CHAR_COMMAND_UUID_16); 
    ESP_LOGI(TAG, "State UUID: 0x%04X", VAN_CHAR_STATE_UUID_16);
    
    rc = ble_gatts_count_cfg(van_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT configuration; rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "GATT configuration counted successfully");
    
    rc = ble_gatts_add_svcs(van_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT services; rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "GATT services added successfully");
    
    // Start GATT services
    rc = ble_gatts_start();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start GATT services; rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "GATT services started successfully");
    
    // Wait for NimBLE to assign handles
    vTaskDelay(pdMS_TO_TICKS(200));
    
    ESP_LOGI(TAG, "Checking assigned handles: command=%d, state=%d", 
             char_command_handle, char_state_handle);
             
    if (char_command_handle == 0 || char_state_handle == 0) {
        ESP_LOGW(TAG, "Handles not assigned yet, they will be captured later");
    } else {
        ESP_LOGI(TAG, "GATT handles properly assigned - services should be visible!");
        ESP_LOGI(TAG, "Final handles: command=%d, state=%d", char_command_handle, char_state_handle);
    }
             
    ESP_LOGI(TAG, "GATT services initialization complete");

    van_advertise();
}

// Called when NimBLE host resets
static void ble_app_on_reset(int reason) {
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

// NimBLE host task function
static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run(); // This function should never return
}

// BLE task
static void ble_task(void *param) {
    char cmd_buffer[MAX_BLE_MESSAGE_SIZE];
    static bool interface_registered = false;
    
    while (1) {
        // Check if we need to register the interface once handles are ready
        if (!interface_registered && char_state_handle != 0) {
            esp_err_t ret = comm_protocol_register_interface(COMM_INTERFACE_BLE, ble_send_data);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "BLE interface registered with communication protocol (handles ready)");
                interface_registered = true;
            } else {
                ESP_LOGE(TAG, "Failed to register BLE interface: %s", esp_err_to_name(ret));
            }
        }
        
        // Wait for commands from BLE
        if (xQueueReceive(ble_command_queue, cmd_buffer, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGC(TAG, "Received BLE command: %s", cmd_buffer);
            
            // Process the received command through the communication protocol
            comm_protocol_process_received_data(COMM_INTERFACE_BLE, cmd_buffer, strlen(cmd_buffer));
        }
        
        // Periodic state broadcasting is handled by the communication protocol task
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t ble_manager_init(void) {
    esp_err_t ret;
    
    // Initialize connections array
    memset(connections, 0, sizeof(connections));
    active_connections = 0;
    
    // Create command queue
    ble_command_queue = xQueueCreate(10, MAX_BLE_MESSAGE_SIZE);
    if (ble_command_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create BLE command queue");
        return ESP_FAIL;
    }
    
    // Initialize NVS for NimBLE
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize NimBLE
    nimble_port_init();
    
    // Configure the host
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.reset_cb = ble_app_on_reset;
    ble_hs_cfg.gatts_register_cb = van_gatt_register_cb;
    
    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task);
    
    // Create BLE manager task
    if (xTaskCreate(ble_task, "ble_task", 8192, NULL, 5, &ble_task_handle) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create BLE task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "BLE Manager initialized successfully");
    return ESP_OK;
}

// Send data to smartphones (Van Management service)
static esp_err_t ble_send_to_smartphones(const char* data, size_t length) {
    if (char_state_handle == 0) {
        ESP_LOGE(TAG, "Van state characteristic handle not ready");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t result = ESP_OK;
    
    // Send to all smartphone connections
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].connected && connections[i].is_smartphone) {
            ESP_LOGI(TAG, "📱 Sending to smartphone slot %d (handle=%d)", i, connections[i].conn_handle);
            
            struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
            if (om == NULL) {
                ESP_LOGE(TAG, "Failed to allocate mbuf for smartphone notification");
                result = ESP_ERR_NO_MEM;
                continue;
            }
            
            int rc = ble_gatts_notify_custom(connections[i].conn_handle, char_state_handle, om);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to send notification to smartphone; rc=%d", rc);
                result = ESP_FAIL;
            }
        }
    }
    
    return result;
}

// Send data to ESP32 clients (ESP32 Communication service)
static esp_err_t ble_send_to_esp32_clients(const char* data, size_t length) {
    if (esp32_char_tx_handle == 0) {
        ESP_LOGE(TAG, "ESP32 TX characteristic handle not ready");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t result = ESP_OK;
    
    // Send to all ESP32 client connections
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].connected && !connections[i].is_smartphone) {
            ESP_LOGI(TAG, "🔌 Sending to ESP32 client slot %d (handle=%d)", i, connections[i].conn_handle);
            
            struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
            if (om == NULL) {
                ESP_LOGE(TAG, "Failed to allocate mbuf for ESP32 notification");
                result = ESP_ERR_NO_MEM;
                continue;
            }
            
            int rc = ble_gatts_notify_custom(connections[i].conn_handle, esp32_char_tx_handle, om);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to send notification to ESP32 client; rc=%d", rc);
                result = ESP_FAIL;
            }
        }
    }
    
    return result;
}

// Original function - backward compatibility (sends to smartphones)
static esp_err_t ble_send_data(const char* data, size_t length) {
    if (!ble_connected) {
        ESP_LOGW(TAG, "BLE not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "📱 Sending data to smartphones (%d bytes): %.100s%s", 
             (int)length, data, length > 100 ? "..." : "");
    
    return ble_send_to_smartphones(data, length);
}

// Nouvelles fonctions publiques pour la communication ESP32
esp_err_t ble_manager_send_to_esp32(const char* data) {
    return ble_send_to_esp32_clients(data, strlen(data));
}

esp_err_t ble_manager_send_to_smartphones(const char* data) {
    return ble_send_to_smartphones(data, strlen(data));
}

// Fonction pour obtenir le nombre de connexions par type
int ble_manager_get_smartphone_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].connected && connections[i].is_smartphone) {
            count++;
        }
    }
    return count;
}

int ble_manager_get_esp32_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].connected && !connections[i].is_smartphone) {
            count++;
        }
    }
    return count;
}

esp_err_t ble_manager_send_state(const char* state_json) {
    return ble_send_data(state_json, strlen(state_json));
}

bool ble_manager_is_connected(void) {
    return ble_connected;
}

esp_err_t ble_manager_deinit(void) {
    if (ble_task_handle) {
        vTaskDelete(ble_task_handle);
        ble_task_handle = NULL;
    }
    
    if (ble_command_queue) {
        vQueueDelete(ble_command_queue);
        ble_command_queue = NULL;
    }
    
    nimble_port_stop();
    nimble_port_deinit();
    
    ESP_LOGI(TAG, "BLE Manager deinitialized");
    return ESP_OK;
}
