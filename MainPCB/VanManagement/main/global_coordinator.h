#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define GC_MAX_SUBSCRIBERS 8

typedef enum {
    GC_EVT_SWITCH_SHORT_CLICK,
    GC_EVT_SWITCH_LONG_PRESS_VALUE,
    GC_EVT_SWITCH_LONG_PRESS_FINAL,
    GC_EVT_DOOR_VALUE_CHANGED,
    // ajouter d'autres types ici
} gc_event_type_t;

typedef struct {
    gc_event_type_t type;
    int value; // valeur générique
} gc_event_t;

// Initialisation du module
esp_err_t global_coordinator_init(void);

// Publier un événement
esp_err_t global_coordinator_publish(gc_event_type_t type, int value);

// S’abonner à un type d’événement
typedef void (*gc_event_callback_t)(gc_event_t evt);
esp_err_t global_coordinator_subscribe(gc_event_type_t type, gc_event_callback_t cb);
