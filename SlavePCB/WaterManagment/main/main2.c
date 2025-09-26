#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <string.h>
#include "w5500_comm.h"

static const char *remote_ip = "192.168.1.11";  // Adaptez selon le sens
static uint16_t port = 1234;

// Tâche pour ESP A (serveur)
static void task_esp_a(void *arg) {
    w5500_comm_t comm = {0};
    if (w5500_comm_init(&comm, 1, remote_ip, port) != ESP_OK) {
        ESP_LOGE("MAIN", "Init ESP A failed");
        vTaskDelete(NULL);
    }

    char buf[256];
    size_t len;
    while (1) {
        // Recevez
        if (w5500_comm_recv(&comm, buf, sizeof(buf), &len) == ESP_OK) {
            // Répondez
            char response[] = "Hello from ESP A!";
            w5500_comm_send(&comm, response, strlen(response));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    w5500_comm_deinit(&comm);
    vTaskDelete(NULL);
}

// Tâche pour ESP B (client) - identique mais is_server=0 et IP distante 192.168.1.10
// Cette fonction est utilisée conditionnellement selon la configuration
static void task_esp_b(void *arg) {
    w5500_comm_t comm = {0};
    const char *remote_ip_b = "192.168.1.10";  // IP du serveur
    if (w5500_comm_init(&comm, 0, remote_ip_b, port) != ESP_OK) {
        ESP_LOGE("MAIN", "Init ESP B failed");
        vTaskDelete(NULL);
    }

    char buf[256];
    size_t len;
    while (1) {
        // Envoyez
        char msg[] = "Hello from ESP B!";
        w5500_comm_send(&comm, msg, strlen(msg));

        // Recevez
        if (w5500_comm_recv(&comm, buf, sizeof(buf), &len) == ESP_OK) {
            // Répondez (optionnel)
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    w5500_comm_deinit(&comm);
    vTaskDelete(NULL);
}

void app_main(void) {
    // Instanciez selon l'ESP (changez dans le code ou via paramètre)
    // Pour ESP A :
    xTaskCreate(task_esp_a, "task_esp_a", 8192, NULL, 5, NULL);
    // Pour ESP B :
    // xTaskCreate(task_esp_b, "task_esp_b", 8192, NULL, 5, NULL);
}