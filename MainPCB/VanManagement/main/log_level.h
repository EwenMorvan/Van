#ifndef LOG_LEVEL_H
#define LOG_LEVEL_H

#include <esp_log.h> // Inclure esp_log.h pour les définitions de base

#define INFO_LOG 1   // Set to 1 to enable info logging, 0 to disable
#define DEBUG_LOG 1  // Set to 1 to enable debug logging, 0 to disable
#define WARN_LOG 1   // Set to 1 to enable warning logging, 0 to disable
#define ERROR_LOG 1  // Set to 1 to enable error logging, 0 to disable
#define CUSTOM_LOG 1 // Set to 1 to enable custom logging, 0 to disable


#if CUSTOM_LOG
    #define ESP_LOGC(TAG, fmt, ...) esp_log_write(ESP_LOG_WARN, TAG, "\033[0;34m[CUSTOM] %s: " fmt "\033[0m\n", TAG, ##__VA_ARGS__)
#else
    #define ESP_LOGC(TAG, fmt, ...) do {} while (0) // Macro vide si CUSTOM_LOG est désactivé
#endif

// Redéfinition de ESP_LOGD
#if !DEBUG_LOG
    #undef ESP_LOGD
    #define ESP_LOGD(TAG, fmt, ...) do {} while (0) // Macro vide si DEBUG_LOG est désactivé
#endif

// Redéfinition de ESP_LOGI
#if !INFO_LOG
    #undef ESP_LOGI
    #define ESP_LOGI(TAG, fmt, ...) do {} while (0) // Macro vide si INFO_LOG est désactivé
#endif

// Redéfinition de ESP_LOGW
#if !WARN_LOG
    #undef ESP_LOGW
    #define ESP_LOGW(TAG, fmt, ...) do {} while (0) // Macro vide si WARN_LOG est désactivé
#endif

// Redéfinition de ESP_LOGE
#if !ERROR_LOG
    #undef ESP_LOGE
    #define ESP_LOGE(TAG, fmt, ...) do {} while (0) // Macro vide si ERROR_LOG est désactivé
#endif


#endif // LOG_LEVEL_H