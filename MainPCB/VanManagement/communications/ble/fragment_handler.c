// fragment_handler.c
#include "fragment_handler.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "FRAGMENT";

#define MAX_FRAGMENT_SIZE 32768  // Taille max d'une commande complète

void fragment_handler_init(fragment_handler_t* handler, uint32_t timeout_ms) {
    memset(handler, 0, sizeof(fragment_handler_t));
    handler->timeout_ms = timeout_ms;
}

void fragment_handler_cleanup(fragment_handler_t* handler) {
    if (handler->assembly.buffer != NULL) {
        free(handler->assembly.buffer);
        handler->assembly.buffer = NULL;
    }
    handler->assembly.active = false;
}

bool fragment_handler_is_active(fragment_handler_t* handler) {
    return handler->assembly.active;
}

void fragment_handler_check_timeout(fragment_handler_t* handler, uint32_t current_ms) {
    if (handler->assembly.active) {
        uint32_t elapsed = current_ms - handler->assembly.last_update_ms;
        if (elapsed > handler->timeout_ms) {
            ESP_LOGW(TAG, "⏱️ Timeout réassemblage (fragment_id=%d, reçu=%d/%d)",
                     handler->assembly.fragment_id,
                     handler->assembly.fragments_received,
                     handler->assembly.total_fragments);
            fragment_handler_cleanup(handler);
        }
    }
}

fragment_result_t fragment_handler_process(
    fragment_handler_t* handler,
    const uint8_t* data,
    size_t len,
    uint8_t** output_data,
    size_t* output_len
) {
    if (data == NULL || len == 0) {
        return FRAGMENT_RESULT_ERROR_INVALID;
    }
    
    // Lire le type de paquet
    packet_type_t packet_type = (packet_type_t)data[0];
    
    // ===== CAS 1: PAQUET COMPLET (pas de fragmentation) =====
    if (packet_type == PACKET_TYPE_COMPLETE) {
        ESP_LOGD(TAG, "📦 Paquet complet (%d bytes)", len - 1);
        *output_data = (uint8_t*)(data + 1);  // Sauter l'en-tête (1 byte)
        *output_len = len - 1;
        return FRAGMENT_RESULT_COMPLETE;
    }
    
    // ===== CAS 2: PREMIER FRAGMENT =====
    if (packet_type == PACKET_TYPE_FIRST_FRAGMENT) {
        if (len < 9) {  // Vérifier taille minimale de l'en-tête
            ESP_LOGE(TAG, "❌ Premier fragment trop petit");
            return FRAGMENT_RESULT_ERROR_INVALID;
        }
        
        // Nettoyer un éventuel réassemblage précédent
        if (handler->assembly.active) {
            ESP_LOGW(TAG, "⚠️ Nouveau fragment reçu, abandon du précédent");
            fragment_handler_cleanup(handler);
        }
        
        // Lire l'en-tête du premier fragment
        uint16_t fragment_id = (data[1] | (data[2] << 8));
        uint16_t total_fragments = (data[3] | (data[4] << 8));
        uint32_t total_size = (data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24));
        
        ESP_LOGI(TAG, "📦 Premier fragment: id=%d, total=%d fragments, taille=%d bytes",
                 fragment_id, total_fragments, total_size);
        
        // Vérifier la taille
        if (total_size > MAX_FRAGMENT_SIZE) {
            ESP_LOGE(TAG, "❌ Taille totale trop grande: %d bytes (max %d)", 
                     total_size, MAX_FRAGMENT_SIZE);
            return FRAGMENT_RESULT_ERROR_INVALID;
        }
        
        // Allouer le buffer
        handler->assembly.buffer = (uint8_t*)malloc(total_size);
        if (handler->assembly.buffer == NULL) {
            ESP_LOGE(TAG, "❌ Échec allocation mémoire (%d bytes)", total_size);
            return FRAGMENT_RESULT_ERROR_MEMORY;
        }
        
        // Initialiser l'état
        handler->assembly.fragment_id = fragment_id;
        handler->assembly.total_fragments = total_fragments;
        handler->assembly.total_size = total_size;
        handler->assembly.fragments_received = 1;
        handler->assembly.current_size = 0;
        handler->assembly.active = true;
        handler->assembly.last_update_ms = esp_log_timestamp();
        
        // Copier les données du premier fragment
        size_t data_size = len - 9;
        memcpy(handler->assembly.buffer, data + 9, data_size);
        handler->assembly.current_size = data_size;
        
        ESP_LOGD(TAG, "✅ Fragment 1/%d reçu (%d bytes de données)",
                 total_fragments, data_size);
        
        // Si c'était le seul fragment
        if (total_fragments == 1) {
            *output_data = handler->assembly.buffer;
            *output_len = handler->assembly.current_size;
            handler->assembly.active = false;
            return FRAGMENT_RESULT_COMPLETE;
        }
        
        return FRAGMENT_RESULT_INCOMPLETE;
    }
    
    // ===== CAS 3 & 4: FRAGMENTS SUIVANTS =====
    if (packet_type == PACKET_TYPE_MIDDLE_FRAGMENT || packet_type == PACKET_TYPE_LAST_FRAGMENT) {
        if (!handler->assembly.active) {
            ESP_LOGE(TAG, "❌ Fragment reçu sans premier fragment");
            return FRAGMENT_RESULT_ERROR_INVALID;
        }
        
        if (len < 5) {
            ESP_LOGE(TAG, "❌ Fragment trop petit");
            return FRAGMENT_RESULT_ERROR_INVALID;
        }
        
        // Lire l'en-tête
        uint16_t fragment_id = (data[1] | (data[2] << 8));
        uint16_t fragment_index = (data[3] | (data[4] << 8));
        
        // Vérifier l'ID
        if (fragment_id != handler->assembly.fragment_id) {
            ESP_LOGE(TAG, "❌ ID fragment incorrect: reçu %d, attendu %d",
                     fragment_id, handler->assembly.fragment_id);
            return FRAGMENT_RESULT_ERROR_INVALID;
        }
        
        // Mettre à jour le timestamp
        handler->assembly.last_update_ms = esp_log_timestamp();
        
        // Copier les données
        size_t data_size = len - 5;
        if (handler->assembly.current_size + data_size > handler->assembly.total_size) {
            ESP_LOGE(TAG, "❌ Débordement buffer: current=%d + new=%d > total=%d",
                     handler->assembly.current_size, data_size, handler->assembly.total_size);
            fragment_handler_cleanup(handler);
            return FRAGMENT_RESULT_ERROR_INVALID;
        }
        
        memcpy(handler->assembly.buffer + handler->assembly.current_size, data + 5, data_size);
        handler->assembly.current_size += data_size;
        handler->assembly.fragments_received++;
        
        ESP_LOGD(TAG, "✅ Fragment %d/%d reçu (%d bytes, total=%d/%d)",
                 fragment_index + 1,
                 handler->assembly.total_fragments,
                 data_size,
                 handler->assembly.current_size,
                 handler->assembly.total_size);
        
        // Vérifier si c'est le dernier
        if (packet_type == PACKET_TYPE_LAST_FRAGMENT) {
            if (handler->assembly.current_size != handler->assembly.total_size) {
                ESP_LOGW(TAG, "⚠️ Taille finale incorrecte: %d != %d",
                         handler->assembly.current_size, handler->assembly.total_size);
            }
            
            ESP_LOGI(TAG, "🎉 Réassemblage complet: %d bytes en %d fragments",
                     handler->assembly.current_size, handler->assembly.fragments_received);
            
            *output_data = handler->assembly.buffer;
            *output_len = handler->assembly.current_size;
            handler->assembly.active = false;
            return FRAGMENT_RESULT_COMPLETE;
        }
        
        return FRAGMENT_RESULT_INCOMPLETE;
    }
    
    ESP_LOGE(TAG, "❌ Type de paquet inconnu: 0x%02x", packet_type);
    return FRAGMENT_RESULT_ERROR_INVALID;
}
