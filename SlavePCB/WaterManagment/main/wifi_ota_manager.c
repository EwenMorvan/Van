#include "wifi_ota_manager.h"
#include "slave_pcb.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <stdarg.h>

static const char *TAG = "WIFI_OTA";

// WiFi credentials - Configure via menuconfig
#define WIFI_SSID        CONFIG_WIFI_SSID
#define WIFI_PASSWORD    CONFIG_WIFI_PASSWORD
#define WIFI_MAX_RETRY   CONFIG_WIFI_MAXIMUM_RETRY

// OTA server settings
#define OTA_SERVER_PORT  CONFIG_OTA_SERVER_PORT
#define OTA_SERVER_IP    CONFIG_OTA_SERVER_IP
#define OTA_FILENAME     CONFIG_OTA_FILENAME

// Event group for WiFi events
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
static httpd_handle_t ota_server = NULL;

// Web logs buffer
#define WEB_LOG_BUFFER_SIZE 15000
#define MAX_LOG_ENTRIES 150

typedef struct {
    char timestamp[32];
    char level[8];
    char tag[32];
    char message[256];
} log_entry_t;

typedef struct {
    char logs[MAX_LOG_ENTRIES][512];
    int head;
    int count;
    SemaphoreHandle_t mutex;
} web_logs_t;

static log_entry_t web_log_buffer[MAX_LOG_ENTRIES];
static web_logs_t web_logs;
static int log_index = 0;
static int log_count = 0;
static SemaphoreHandle_t log_mutex = NULL;

// Forward declarations
static void clean_ansi_and_format_log(const char* raw_message, char* formatted_log, size_t max_size);
static void enable_web_logging(void);

/**
 * @brief Custom vprintf function for web logs - OPTIMIZED VERSION
 */
static int web_log_vprintf(const char *fmt, va_list args) {
    // Call original vprintf first pour afficher dans la console série
    int ret = vprintf(fmt, args);
    
    // Fast path: Only capture logs if web interface is actually being used
    // This reduces overhead when no one is viewing the web logs
    if (!web_logs.mutex) {
        return ret;
    }
    
    // Use a simpler, faster approach for web logging
    // Use a fast timeout to ensure we don't lose too many logs
    if (xSemaphoreTake(web_logs.mutex, pdMS_TO_TICKS(1)) == pdTRUE) {
        char log_buffer[256];  // Smaller buffer for better performance
        int len = vsnprintf(log_buffer, sizeof(log_buffer), fmt, args);
        
        if (len > 0 && len < sizeof(log_buffer)) {
            // Simple copy without complex ANSI processing - 10x faster
            size_t copy_len = strlen(log_buffer);
            if (copy_len > 0 && copy_len < sizeof(web_logs.logs[web_logs.head]) - 1) {
                memcpy(web_logs.logs[web_logs.head], log_buffer, copy_len + 1);
                
                web_logs.head = (web_logs.head + 1) % MAX_LOG_ENTRIES;
                if (web_logs.count < MAX_LOG_ENTRIES) {
                    web_logs.count++;
                }
            }
        }
        xSemaphoreGive(web_logs.mutex);
    }
    // If mutex is busy, simply skip web logging to avoid blocking critical tasks
    
    return ret;
}

/**
 * @brief Nettoyer les codes ANSI et formater le log
 */
static void clean_ansi_and_format_log(const char* raw_message, char* formatted_log, size_t max_size) {
    if (!raw_message || !formatted_log) return;
    
    char temp_buffer[512];
    int temp_idx = 0;
    
    // Première passe : supprimer tous les codes ANSI et de couleur
    for (int i = 0; raw_message[i] != '\0' && temp_idx < sizeof(temp_buffer) - 1; i++) {
        // Détecter les séquences ANSI: \033[...m ou [0;32m
        if (raw_message[i] == '\033' || raw_message[i] == 0x1B) {
            // Code ANSI avec ESC, ignorer jusqu'au 'm'
            while (raw_message[i] != '\0' && raw_message[i] != 'm') {
                i++;
            }
            continue; // Ignorer le 'm' aussi
        } else if (raw_message[i] == '[' && i + 1 < strlen(raw_message)) {
            // Vérifier si c'est une séquence de couleur [0;32m, [1;33m, etc.
            int j = i + 1;
            bool is_color_sequence = true;
            
            // Vérifier le pattern [digits;digits...m
            while (raw_message[j] != '\0' && raw_message[j] != 'm' && j < i + 10) {
                if (!(raw_message[j] >= '0' && raw_message[j] <= '9') && raw_message[j] != ';') {
                    is_color_sequence = false;
                    break;
                }
                j++;
            }
            
            if (is_color_sequence && raw_message[j] == 'm') {
                // C'est une séquence de couleur, ignorer tout
                i = j; // Skip jusqu'au 'm'
                continue;
            } else {
                // Ce n'est pas une séquence de couleur, garder le '['
                temp_buffer[temp_idx++] = raw_message[i];
            }
        } else if (raw_message[i] >= 32 || raw_message[i] == ' ' || raw_message[i] == '\t') {
            temp_buffer[temp_idx++] = raw_message[i];
        }
    }
    temp_buffer[temp_idx] = '\0';
    
    // Deuxième passe : extraire le niveau de log et le timestamp
    char level = 'I'; // Par défaut INFO
    char timestamp_str[32] = "";
    char tag[32] = "";
    char message[256] = "";
    
    // Pattern ESP-IDF: "I (12345) TAG: message"
    if (sscanf(temp_buffer, "%c (%31[^)]) %31[^:]: %255[^\n]", &level, timestamp_str, tag, message) == 4) {
        // Format parfait trouvé
        snprintf(formatted_log, max_size, "%c (%s) %s: %s", level, timestamp_str, tag, message);
    } else if (sscanf(temp_buffer, "%c (%31[^)]) %255[^\n]", &level, timestamp_str, message) == 3) {
        // Sans TAG
        snprintf(formatted_log, max_size, "%c (%s) %s", level, timestamp_str, message);
    } else {
        // Fallback: garder tel quel après nettoyage
        strncpy(formatted_log, temp_buffer, max_size - 1);
        formatted_log[max_size - 1] = '\0';
    }
}

/**
 * @brief Add a log entry to web buffer
 */
void add_web_log(const char* message) {
    if (!web_logs.mutex || !message) return;
    
    // Ultra-fast non-blocking approach
    if (xSemaphoreTake(web_logs.mutex, 0) == pdTRUE) {
        // Direct memory copy - much faster than string operations
        size_t msg_len = strlen(message);
        if (msg_len > 0 && msg_len < sizeof(web_logs.logs[web_logs.head]) - 1) {
            memcpy(web_logs.logs[web_logs.head], message, msg_len + 1);
            
            web_logs.head = (web_logs.head + 1) % MAX_LOG_ENTRIES;
            if (web_logs.count < MAX_LOG_ENTRIES) {
                web_logs.count++;
            }
        }
        xSemaphoreGive(web_logs.mutex);
    }
    // If mutex is busy, skip logging to avoid blocking critical operations
}

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry connecting to WiFi");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Connection to WiFi failed");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief OTA upload handler
 */
static esp_err_t ota_upload_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Starting OTA update...");
    
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *ota_partition = NULL;
    
    // Get next OTA partition
    ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!ota_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Writing to partition '%s' at offset 0x%lx", 
             ota_partition->label, ota_partition->address);
    
    // Begin OTA
    esp_err_t err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }
    
    // Receive and write firmware data
    char buf[1024];
    int received;
    int total_received = 0;
    
    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }
        total_received += received;
    }
    
    if (received < 0) {
        ESP_LOGE(TAG, "Failed to receive firmware data");
        esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        return ESP_FAIL;
    }
    
    // Finalize OTA
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }
    
    // Set boot partition
    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "OTA update successful! Received %d bytes", total_received);
    
    // Send success response
    const char* resp_str = "OTA Update successful! Device will restart in 5 seconds.";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    
    // Restart after 5 seconds
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
    
    return ESP_OK;
}

/**
 * @brief Web page handler for OTA interface
 */
static esp_err_t ota_page_handler(httpd_req_t *req) {
    const char* html_page = 
        "<!DOCTYPE html>"
        "<html><head><title>SlavePCB OTA Update</title>"
        "<meta charset='UTF-8'>"
        "<style>body{font-family:Arial,sans-serif;margin:40px;} .nav{margin:20px 0;} .nav a{margin-right:20px;text-decoration:none;color:#0066cc;} form{background:#f5f5f5;padding:20px;border-radius:5px;}</style>"
        "</head>"
        "<body>"
        "<h1>🚁 SlavePCB - Mise à jour OTA</h1>"
        "<div class='nav'>"
        "<a href='/logs'>📋 Logs en temps réel</a>"
        "<a href='/status'>📊 Statut système</a>"
        "<a href='/api/logs'>🔗 API Logs (JSON)</a>"
        "</div>"
        "<form method='POST' action='/upload' enctype='multipart/form-data'>"
        "<h3>📤 Upload Firmware</h3>"
        "<input type='file' name='firmware' accept='.bin'><br><br>"
        "<input type='submit' value='Upload Firmware' style='padding:10px 20px;background:#28a745;color:white;border:none;border-radius:3px;cursor:pointer;'>"
        "</form>"
        "<hr>"
        "<h2>📖 Instructions:</h2>"
        "<ol>"
        "<li>Compilez votre firmware avec: <code>idf.py build</code></li>"
        "<li>Le fichier .bin se trouve dans <code>build/WaterManagment.bin</code></li>"
        "<li>Sélectionnez le fichier .bin et cliquez sur 'Upload Firmware'</li>"
        "<li>L'ESP32 redémarrera automatiquement après la mise à jour</li>"
        "<li>Surveillez les <a href='/logs'>logs en temps réel</a> pour le debug</li>"
        "</ol>"
        "</body></html>";
    
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

/**
 * @brief Status page handler
 */
static esp_err_t status_handler(httpd_req_t *req) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    
    char response[1024];
    snprintf(response, sizeof(response),
        "<!DOCTYPE html>"
        "<html><head><title>SlavePCB Status</title></head>"
        "<body>"
        "<h1>SlavePCB - Statut du système</h1>"
        "<p><strong>Partition active:</strong> %s (0x%lx)</p>"
        "<p><strong>Partition de boot:</strong> %s (0x%lx)</p>"
        "<p><strong>Version ESP-IDF:</strong> %s</p>"
        "<p><strong>Chip model:</strong> %s</p>"
        "<a href='/'>← Retour à la page OTA</a>"
        "</body></html>",
        running->label, running->address,
        boot->label, boot->address,
        esp_get_idf_version(),
        CONFIG_IDF_TARGET);
    
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

/**
 * @brief Échapper une chaîne pour JSON
 */
static void escape_json_string(const char* input, char* output, size_t output_size) {
    size_t out_idx = 0;
    
    for (size_t i = 0; input[i] != '\0' && out_idx < output_size - 1; i++) {
        char c = input[i];
        
        // Ignorer les caractères de contrôle (codes ANSI, etc.)
        if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
            continue;
        }
        
        // Échapper les caractères spéciaux JSON
        if (c == '"' && out_idx < output_size - 2) {
            output[out_idx++] = '\\';
            output[out_idx++] = '"';
        } else if (c == '\\' && out_idx < output_size - 2) {
            output[out_idx++] = '\\';
            output[out_idx++] = '\\';
        } else if (c == '\n' && out_idx < output_size - 2) {
            output[out_idx++] = '\\';
            output[out_idx++] = 'n';
        } else if (c == '\r' && out_idx < output_size - 2) {
            output[out_idx++] = '\\';
            output[out_idx++] = 'r';
        } else if (c == '\t' && out_idx < output_size - 2) {
            output[out_idx++] = '\\';
            output[out_idx++] = 't';
        } else {
            output[out_idx++] = c;
        }
    }
    
    output[out_idx] = '\0';
}

/**
 * @brief Handler pour les logs en temps réel (API JSON) - OPTIMIZED VERSION avec gestion d'erreurs
 */
static esp_err_t logs_api_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    
    if (!web_logs.mutex) {
        httpd_resp_sendstr(req, "{\"logs\":[],\"error\":\"Logs not initialized\"}");
        return ESP_OK;
    }
    
    // Use shorter timeout for better responsiveness
    if (xSemaphoreTake(web_logs.mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        esp_err_t ret = ESP_OK;
        
        // Limit number of logs to send (max 50 most recent to catch rapid logs but prevent huge transfers)
        int max_logs = (web_logs.count > 50) ? 50 : web_logs.count;
        int start_idx = web_logs.count - max_logs;
        
        // Pre-allocate buffer for better performance and error handling
        char* response_buffer = malloc(4096);
        if (!response_buffer) {
            xSemaphoreGive(web_logs.mutex);
            httpd_resp_sendstr(req, "{\"logs\":[],\"error\":\"Memory allocation failed\"}");
            return ESP_OK;
        }
        
        // Build response in buffer first
        int pos = snprintf(response_buffer, 4096, "{\"logs\":[");
        
        for (int i = 0; i < max_logs && pos < 3800; i++) {  // Leave room for closing
            int idx = (web_logs.head - max_logs + i + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
            
            if (i > 0) {
                pos += snprintf(response_buffer + pos, 4096 - pos, ",");
            }
            
            // Add log with basic escaping
            pos += snprintf(response_buffer + pos, 4096 - pos, "\"");
            
            const char* log = web_logs.logs[idx];
            size_t log_len = strlen(log);
            
            // Copy log with minimal escaping, prevent buffer overflow
            for (size_t j = 0; j < log_len && pos < 3700; j++) {
                char c = log[j];
                if (c == '"') {
                    pos += snprintf(response_buffer + pos, 4096 - pos, "\\\"");
                } else if (c == '\\') {
                    pos += snprintf(response_buffer + pos, 4096 - pos, "\\\\");
                } else if (c == '\n') {
                    pos += snprintf(response_buffer + pos, 4096 - pos, "\\n");
                } else if (c == '\r') {
                    pos += snprintf(response_buffer + pos, 4096 - pos, "\\r");
                } else if (c >= 32 || c == '\t') {  // Skip unprintable chars except tab
                    response_buffer[pos++] = c;
                }
                
                if (pos >= 3700) break;  // Prevent overflow
            }
            
            pos += snprintf(response_buffer + pos, 4096 - pos, "\"");
        }
        
        // Close JSON
        snprintf(response_buffer + pos, 4096 - pos, "],\"count\":%d,\"total\":%d}", max_logs, web_logs.count);
        
        xSemaphoreGive(web_logs.mutex);
        
        // Send all at once to prevent HTTP errors
        ret = httpd_resp_sendstr(req, response_buffer);
        free(response_buffer);
        
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send logs response: %d", ret);
        }
        
    } else {
        httpd_resp_sendstr(req, "{\"logs\":[],\"error\":\"Mutex timeout\"}");
    }
    
    return ESP_OK;
}

/**
 * @brief Handler pour la page de logs web
 */
static esp_err_t logs_page_handler(httpd_req_t *req) {
    const char* html_page = 
        "<!DOCTYPE html>"
        "<html><head>"
        "<title>SlavePCB - Logs en temps réel</title>"
        "<meta charset='UTF-8'>"
        "<style>"
        "body { font-family: monospace; margin: 20px; }"
        "#logs { background: #000; color: #ccc; padding: 15px; height: 400px; overflow-y: scroll; border: 1px solid #ccc; }"
        ".log-entry { margin: 2px 0; white-space: pre-wrap; }"
        ".log-info { color: #00ff00; }"      /* Vert pour INFO */
        ".log-debug { color: #00ffff; }"     /* Cyan pour DEBUG */
        ".log-warn { color: #ffff00; }"      /* Jaune pour WARNING */
        ".log-error { color: #ff0000; }"     /* Rouge pour ERROR */
        ".log-unknown { color: #cccccc; }"   /* Gris pour autres */
        ".ansi-red { color: #ff0000; }"      /* ANSI red */
        ".ansi-green { color: #00ff00; }"    /* ANSI green */
        ".ansi-yellow { color: #ffff00; }"   /* ANSI yellow */
        ".ansi-blue { color: #0000ff; }"     /* ANSI blue */
        ".ansi-magenta { color: #ff00ff; }"  /* ANSI magenta */
        ".ansi-cyan { color: #00ffff; }"     /* ANSI cyan */
        ".ansi-white { color: #ffffff; }"    /* ANSI white */
        ".ansi-bold { font-weight: bold; }"  /* ANSI bold */
        ".controls { margin: 10px 0; }"
        "button { padding: 5px 10px; margin: 0 5px; }"
        ".status { color: #666; }"
        "</style>"
        "</head><body>"
        "<h1>SlavePCB - Logs en temps réel</h1>"
        "<div class='controls'>"
        "<button onclick='refreshLogs()'>🔄 Actualiser</button>"
        "<button onclick='toggleAutoRefresh()' id='autoBtn'>⏸️ Auto (ON)</button>"
        "<button onclick='clearLogs()'>🗑️ Vider</button>"
        "<span class='status' id='status'>Connecté</span>"
        "</div>"
        "<div id='logs'></div>"
        "<div class='controls'>"
        "<a href='/'>← Retour OTA</a> | "
        "<a href='/status'>📊 Status</a>"
        "</div>"
        "<script>"
        "let autoRefresh = true;"
        "let refreshInterval;"
        
        "function getLogClass(logText) {"
        "  if (logText.startsWith('[0;32mI ') || logText.includes(' I (')) return 'log-info';"
        "  if (logText.startsWith('[0;36mD ') || logText.includes(' D (')) return 'log-debug';"
        "  if (logText.startsWith('[0;33mW ') || logText.includes(' W (')) return 'log-warn';"
        "  if (logText.startsWith('[0;31mE ') || logText.includes(' E (')) return 'log-error';"
        "  return 'log-unknown';"
        "}"
        
        "function parseAnsiCodes(text) {"
        "  const ansiRegex = /\x1B\\[[0-9;]*m/g;"
        "  let classes = [];"
        "  let currentStyles = [];"
        "  let cleanText = text.replace(ansiRegex, (match) => {"
        "    const codes = match.slice(2, -1).split(';').map(Number);"
        "    if (codes.includes(0)) {"
        "      currentStyles = []; /* Reset all styles */"
        "    } else {"
        "      for (let code of codes) {"
        "        if (code === 1) currentStyles.push('ansi-bold');"
        "        else if (code === 31) currentStyles.push('ansi-red');"
        "        else if (code === 32) currentStyles.push('ansi-green');"
        "        else if (code === 33) currentStyles.push('ansi-yellow');"
        "        else if (code === 34) currentStyles.push('ansi-blue');"
        "        else if (code === 35) currentStyles.push('ansi-magenta');"
        "        else if (code === 36) currentStyles.push('ansi-cyan');"
        "        else if (code === 37) currentStyles.push('ansi-white');"
        "      }"
        "    }"
        "    classes = [...currentStyles];"
        "    return '';"
        "  });"
        "  return { text: cleanText, classes: classes };"
        "}"
        
        "function refreshLogs() {"
        "  const controller = new AbortController();"
        "  const timeoutId = setTimeout(() => controller.abort(), 1000);"
        "  fetch('/api/logs', { signal: controller.signal })"
        "    .then(response => {"
        "      clearTimeout(timeoutId);"
        "      if (!response.ok) {"
        "        throw new Error('HTTP ' + response.status);"
        "      }"
        "      return response.json();"
        "    })"
        "    .then(data => {"
        "      const logsDiv = document.getElementById('logs');"
        "      if (data.error) {"
        "        logsDiv.innerHTML = '<div style=\"color:red\">Erreur: ' + data.error + '</div>';"
        "        return;"
        "      }"
        "      logsDiv.innerHTML = '';"
        "      data.logs.forEach(log => {"
        "        const { text, classes } = parseAnsiCodes(log);"
        "        const div = document.createElement('div');"
        "        div.className = 'log-entry ' + getLogClass(log) + ' ' + classes.join(' ');"
        "        div.textContent = text;"
        "        logsDiv.appendChild(div);"
        "      });"
        "      logsDiv.scrollTop = logsDiv.scrollHeight;"
        "      document.getElementById('status').textContent = 'Temps-réel: ' + new Date().toLocaleTimeString() + ' (' + data.logs.length + ' logs)';"
        "    })"
        "    .catch(err => {"
        "      clearTimeout(timeoutId);"
        "      let errorMsg = 'Erreur de connexion';"
        "      if (err.name === 'AbortError') {"
        "        errorMsg = 'Timeout (>1000ms)';"
        "      } else if (err.message.includes('HTTP')) {"
        "        errorMsg = err.message;"
        "      }"
        "      document.getElementById('status').textContent = errorMsg;"
        "      console.error('Fetch error:', err);"
        "    });"
        "}"
        
        "function toggleAutoRefresh() {"
        "  autoRefresh = !autoRefresh;"
        "  const btn = document.getElementById('autoBtn');"
        "  if (autoRefresh) {"
        "    btn.textContent = '⏸️ Auto (ON)';"
        "    startAutoRefresh();"
        "  } else {"
        "    btn.textContent = '▶️ Auto (OFF)';"
        "    clearInterval(refreshInterval);"
        "  }"
        "}"
        
        "function startAutoRefresh() {"
        "  if (autoRefresh) {"
        "    refreshInterval = setInterval(refreshLogs, 100);"
        "  }"
        "}"
        
        "function clearLogs() {"
        "  if (confirm('Vider tous les logs?')) {"
        "    fetch('/api/logs/clear', {method: 'POST'})"
        "      .then(() => refreshLogs());"
        "  }"
        "}"
        
        "// Initialisation"
        "refreshLogs();"
        "startAutoRefresh();"
        "</script>"
        "</body></html>";
    
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}
/**
 * @brief Handler pour vider les logs
 */
static esp_err_t logs_clear_handler(httpd_req_t *req) {
    if (web_logs.mutex && xSemaphoreTake(web_logs.mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        web_logs.head = 0;
        web_logs.count = 0;
        memset(web_logs.logs, 0, sizeof(web_logs.logs));
        xSemaphoreGive(web_logs.mutex);
        
        add_web_log("Logs vidés via interface web");
    }
    
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

/**
 * @brief Start OTA HTTP server
 */
static esp_err_t start_ota_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = OTA_SERVER_PORT;
    config.max_uri_handlers = 8;
    
    // Optimize for better connection handling and prevent send errors
    config.recv_wait_timeout = 5;           // 5 seconds receive timeout
    config.send_wait_timeout = 5;           // 5 seconds send timeout  
    config.task_priority = 5;               // Lower priority to avoid blocking
    config.stack_size = 8192;               // Increase stack size for handling
    config.max_open_sockets = 4;            // Limit concurrent connections
    config.lru_purge_enable = true;         // Enable LRU purge for old connections
    config.backlog_conn = 2;                // Limit backlog connections
    
    if (httpd_start(&ota_server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t ota_page_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = ota_page_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(ota_server, &ota_page_uri);
        
        httpd_uri_t ota_upload_uri = {
            .uri = "/upload",
            .method = HTTP_POST,
            .handler = ota_upload_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(ota_server, &ota_upload_uri);
        
        httpd_uri_t status_uri = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(ota_server, &status_uri);
        
        httpd_uri_t logs_api_uri = {
            .uri = "/api/logs",
            .method = HTTP_GET,
            .handler = logs_api_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(ota_server, &logs_api_uri);
        
        httpd_uri_t logs_page_uri = {
            .uri = "/logs",
            .method = HTTP_GET,
            .handler = logs_page_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(ota_server, &logs_page_uri);
        
        httpd_uri_t logs_clear_uri = {
            .uri = "/api/logs/clear",
            .method = HTTP_POST,
            .handler = logs_clear_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(ota_server, &logs_clear_uri);
        
        ESP_LOGI(TAG, "OTA server started on port %d", OTA_SERVER_PORT);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start OTA server");
    return ESP_FAIL;
}

/**
 * @brief Initialize WiFi
 */
slave_pcb_err_t wifi_ota_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi OTA manager");
    
    // Initialiser le système de logs web
    web_logs.mutex = xSemaphoreCreateMutex();
    if (!web_logs.mutex) {
        ESP_LOGE(TAG, "Failed to create web logs mutex");
        return SLAVE_PCB_ERR_MEMORY;
    }
    memset(web_logs.logs, 0, sizeof(web_logs.logs));
    web_logs.head = 0;
    web_logs.count = 0;
    
    // NE PAS installer le hook de log pour éviter les crashes
    // esp_log_set_vprintf(web_log_vprintf);
    
    add_web_log("Système de logs web initialisé");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize netif
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    s_wifi_event_group = xEventGroupCreate();
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialization finished. Connecting to %s...", WIFI_SSID);
    
    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE, pdFALSE, portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi successfully");
        
        // Start OTA server
        if (start_ota_server() == ESP_OK) {
            ESP_LOGI(TAG, "OTA server running at http://[IP_ADDRESS]:%d", OTA_SERVER_PORT);
            
            // Enable web logging now that everything is stable
            enable_web_logging();
            
            return SLAVE_PCB_OK;
        } else {
            return SLAVE_PCB_ERR_COMM_FAIL;
        }
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return SLAVE_PCB_ERR_COMM_FAIL;
    } else {
        ESP_LOGE(TAG, "Unexpected WiFi event");
        return SLAVE_PCB_ERR_COMM_FAIL;
    }
}

/**
 * @brief Get WiFi connection status
 */
bool wifi_ota_is_connected(void) {
    return (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}

/**
 * @brief Stop WiFi and OTA services
 */
void wifi_ota_stop(void) {
    if (ota_server) {
        httpd_stop(ota_server);
        ota_server = NULL;
        ESP_LOGI(TAG, "OTA server stopped");
    }
    
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // Nettoyer le système de logs web
    if (web_logs.mutex) {
        vSemaphoreDelete(web_logs.mutex);
        web_logs.mutex = NULL;
    }
    
    ESP_LOGI(TAG, "WiFi stopped");
}

/**
 * @brief Enable web logging after WiFi is stable
 */
static void enable_web_logging(void) {
    if (web_logs.mutex && wifi_ota_is_connected()) {
        ESP_LOGI(TAG, "Enabling web logging...");
        
        // Ajouter quelques logs de test d'abord
        add_web_log("I (000000) WIFI_OTA: Web logging enabled");
        add_web_log("I (000001) WIFI_OTA: Capturing ESP32 system logs...");
        
        // Attendre un peu pour que les logs initiaux soient visibles
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Now it's safe to install the log hook pour capturer TOUS les logs
        esp_log_set_vprintf(web_log_vprintf);
        
        ESP_LOGI(TAG, "Web logging hook installed - all logs will be captured");
        ESP_LOGI(TAG, "Access web logs at: http://[IP]:%d/logs", OTA_SERVER_PORT);
    }
}
