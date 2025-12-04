#pragma once
#include "esp_err.h"

// Initialisation du LED coordinator
// S'abonne aux événements du global coordinator pour gérer les LEDs
esp_err_t led_coordinator_init(void);
