// fragment_handler.h
#ifndef FRAGMENT_HANDLER_H
#define FRAGMENT_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Types de paquets
typedef enum {
    PACKET_TYPE_COMPLETE = 0x00,        // Paquet complet (pas de fragmentation)
    PACKET_TYPE_FIRST_FRAGMENT = 0x01,  // Premier fragment
    PACKET_TYPE_MIDDLE_FRAGMENT = 0x02, // Fragment intermédiaire
    PACKET_TYPE_LAST_FRAGMENT = 0x03    // Dernier fragment
} packet_type_t;

// Structure pour stocker un fragment en cours de réassemblage
typedef struct {
    uint16_t fragment_id;
    uint16_t total_fragments;
    uint32_t total_size;
    uint16_t fragments_received;
    uint8_t* buffer;
    size_t current_size;
    bool active;
    uint32_t last_update_ms;
} fragment_assembly_t;

// Résultat du traitement d'un fragment
typedef enum {
    FRAGMENT_RESULT_INCOMPLETE,   // Fragment reçu, en attente des suivants
    FRAGMENT_RESULT_COMPLETE,     // Tous les fragments reçus, données complètes disponibles
    FRAGMENT_RESULT_ERROR_MEMORY, // Erreur d'allocation mémoire
    FRAGMENT_RESULT_ERROR_INVALID,// Fragment invalide
    FRAGMENT_RESULT_ERROR_TIMEOUT // Timeout de réassemblage
} fragment_result_t;

// Gestionnaire de fragments
typedef struct {
    fragment_assembly_t assembly;
    uint32_t timeout_ms;
} fragment_handler_t;

/**
 * Initialise le gestionnaire de fragments
 */
void fragment_handler_init(fragment_handler_t* handler, uint32_t timeout_ms);

/**
 * Traite un paquet reçu (complet ou fragment)
 * 
 * @param handler Gestionnaire de fragments
 * @param data Données reçues
 * @param len Longueur des données
 * @param output_data Pointeur vers les données complètes (si COMPLETE)
 * @param output_len Longueur des données complètes (si COMPLETE)
 * @return Résultat du traitement
 */
fragment_result_t fragment_handler_process(
    fragment_handler_t* handler,
    const uint8_t* data,
    size_t len,
    uint8_t** output_data,
    size_t* output_len
);

/**
 * Nettoie les ressources du gestionnaire
 */
void fragment_handler_cleanup(fragment_handler_t* handler);

/**
 * Vérifie si un réassemblage est en cours
 */
bool fragment_handler_is_active(fragment_handler_t* handler);

/**
 * Vérifie le timeout et nettoie si nécessaire
 */
void fragment_handler_check_timeout(fragment_handler_t* handler, uint32_t current_ms);

#endif // FRAGMENT_HANDLER_H
