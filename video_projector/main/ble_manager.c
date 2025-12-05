#include "ble_manager.h"
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_MANAGER";

// UUIDs définies pour le service vidéoprojecteur
#define VIDEO_PROJ_SERVICE_UUID           0x181A
#define VIDEO_PROJ_CONTROL_CHAR_UUID      0x2A58
#define VIDEO_PROJ_STATUS_CHAR_UUID       0x2A19

static uint16_t g_status_char_handle = 0;
static ble_command_callback_t g_cmd_callback = NULL;
static bool g_is_advertising = false;
static bool g_is_connected = false;
static uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static char g_device_name[32] = "VideoProjector_Van";

/**
 * @brief Callback GATT pour les écritures sur la caractéristique de contrôle
 */
static int control_char_write_callback(uint16_t conn_handle,
                                       uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt *ctxt,
                                       void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return 0;
    }
    
    if (ctxt->om->om_len > 0) {
        uint8_t cmd = ctxt->om->om_data[0];
        
        ESP_LOGI(TAG, "Commande BLE reçue: %d", cmd);
        
        if (g_cmd_callback != NULL) {
            g_cmd_callback((ble_command_t)cmd);
        }
    }
    
    return 0;
}

/**
 * @brief Callback GATT pour les lectures sur la caractéristique de statut
 */
static int status_char_read_callback(uint16_t conn_handle,
                                     uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt,
                                     void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return 0;
    }
    
    // Retourne le statut (à implémenter avec la vraie valeur)
    uint8_t status = 0;  // 0 = rétracté, 1 = déployé
    os_mbuf_append(ctxt->om, &status, sizeof(status));
    
    return 0;
}

/**
 * @brief Définition du service GATT vidéoprojecteur
 */
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        // Service vidéoprojecteur
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(VIDEO_PROJ_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Caractéristique de contrôle (write)
                .uuid = BLE_UUID16_DECLARE(VIDEO_PROJ_CONTROL_CHAR_UUID),
                .flags = BLE_GATT_CHR_F_WRITE,
                .access_cb = control_char_write_callback,
            },
            {
                // Caractéristique de statut (read/notify)
                .uuid = BLE_UUID16_DECLARE(VIDEO_PROJ_STATUS_CHAR_UUID),
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .access_cb = status_char_read_callback,
                .val_handle = &g_status_char_handle,
            },
            {
                0,  // Fin du tableau
            }
        }
    },
    {
        0,  // Fin du tableau
    }
};

/**
 * @brief Callback pour les événements BLE GAP
 */
static int ble_gap_event_callback(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Connexion BLE établie");
            g_is_connected = true;
            g_conn_handle = event->connect.conn_handle;
            break;
            
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Déconnexion BLE");
            g_is_connected = false;
            g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            
            // Redémarre l'advertising
            ble_manager_start_advertising();
            break;
            
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising terminé");
            break;
            
        default:
            break;
    }
    
    return 0;
}

/**
 * @brief Callback pour les événements BLE HOST
 */
static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task démarrée");
    nimble_port_run();
}

/**
 * @brief Callback appelé quand le BLE host est prêt
 */
static void ble_on_sync(void)
{
    int rc;
    
    ESP_LOGI(TAG, "BLE host synchronisé");
    
    // Définit la MTU préférée à 247 octets (max BLE)
    rc = ble_att_set_preferred_mtu(247);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur définition MTU préférée: %d", rc);
    } else {
        ESP_LOGI(TAG, "MTU préférée définie à 247 octets");
    }
    
    // Configure le nom du device AVANT d'ajouter les services
    rc = ble_svc_gap_device_name_set(g_device_name);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur définition nom device: %d", rc);
    }
    
    // Compte et ajoute les services GATT
    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur count GATT services: %d", rc);
        return;
    }
    
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur add GATT services: %d", rc);
        return;
    }
    
    // IMPORTANT: Démarre le serveur GATT (rend les services visibles!)
    rc = ble_gatts_start();
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur start GATT server: %d", rc);
        return;
    }
    
    ESP_LOGI(TAG, "Services GATT démarrés (nom: %s)", g_device_name);
    
    // Démarre l'advertising
    ble_manager_start_advertising();
}

/**
 * @brief Callback appelé quand il y a une erreur BLE
 */
static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset reason: %d", reason);
}

int ble_manager_init(const char *device_name, ble_command_callback_t callback)
{
    if (device_name == NULL) {
        ESP_LOGE(TAG, "Nom device NULL");
        return -1;
    }
    
    // Sauvegarde le callback et le nom du device
    g_cmd_callback = callback;
    strncpy(g_device_name, device_name, sizeof(g_device_name) - 1);
    g_device_name[sizeof(g_device_name) - 1] = '\0';
    
    // Initialise NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Libère la mémoire du BT Classic (ESP32-C3 ne supporte que BLE)
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Libération mémoire BT Classic: %s", esp_err_to_name(ret));
    }
    
    // Initialise NimBLE port (cela initialise automatiquement le contrôleur BLE)
    nimble_port_init();
    
    // Configure les callbacks AVANT de démarrer la tâche
    // Les services GATT seront ajoutés dans ble_on_sync après synchronisation
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;
    
    // Démarre la tâche NimBLE host (gère les événements BLE)
    nimble_port_freertos_init(ble_host_task);
    
    ESP_LOGI(TAG, "Gestionnaire BLE initialisé (nom: %s)", device_name);
    return 0;
}

int ble_manager_start_advertising(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;
    
    // Configure les données d'advertising (nom + flags + service UUID)
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)g_device_name;
    fields.name_len = strlen(g_device_name);
    fields.name_is_complete = 1;
    
    // Ajoute l'UUID du service dans l'advertising
    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(VIDEO_PROJ_SERVICE_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur config advertising data: %d", rc);
        return -1;
    }
    
    // Configure les paramètres d'advertising
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;  // Undirected connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;   // General discoverable
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(100); // 100ms pour meilleure découverte
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(200); // 200ms
    
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event_callback, NULL);
    
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur démarrage advertising: %d", rc);
        return -1;
    }
    
    g_is_advertising = true;
    ESP_LOGI(TAG, "Advertising démarré (nom: %s)", g_device_name);
    return 0;
}

int ble_manager_stop_advertising(void)
{
    int rc = ble_gap_adv_stop();
    
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur arrêt advertising: %d", rc);
        return -1;
    }
    
    g_is_advertising = false;
    ESP_LOGI(TAG, "Advertising arrêté");
    return 0;
}

int ble_manager_notify_status(bool is_deployed)
{
    struct os_mbuf *om;
    uint8_t status = is_deployed ? 1 : 0;
    int rc;
    
    if (!g_is_connected) {
        ESP_LOGW(TAG, "Aucun client BLE connecté");
        return -1;
    }
    
    // Crée un buffer pour la notification
    om = ble_hs_mbuf_from_flat(&status, sizeof(status));
    if (om == NULL) {
        ESP_LOGE(TAG, "Erreur création buffer notification");
        return -1;
    }
    
    // Envoie la notification (ble_gatts_notify_custom pour NimBLE)
    rc = ble_gatts_notify_custom(g_conn_handle, g_status_char_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur envoi notification: %d", rc);
        return -1;
    }
    
    ESP_LOGI(TAG, "Notification statut envoyée (déployé: %s)", 
             is_deployed ? "OUI" : "NON");
    
    return 0;
}

int ble_manager_send_json(const char *json_string)
{
    struct os_mbuf *om;
    int rc;
    
    if (!g_is_connected || json_string == NULL) {
        return -1;
    }
    
    size_t len = strlen(json_string);
    
    // Crée un buffer pour la notification
    om = ble_hs_mbuf_from_flat(json_string, len);
    if (om == NULL) {
        ESP_LOGE(TAG, "Erreur création buffer JSON");
        return -1;
    }
    
    // Envoie la notification
    rc = ble_gatts_notify_custom(g_conn_handle, g_status_char_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erreur envoi JSON: %d", rc);
        return -1;
    }
    
    ESP_LOGD(TAG, "JSON envoyé: %s", json_string);
    return 0;
}

bool ble_manager_is_connected(void)
{
    return g_is_connected;
}
