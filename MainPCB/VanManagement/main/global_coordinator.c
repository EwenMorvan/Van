#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "global_coordinator.h"


static const char *TAG = "GLOBAL_COORD";

// Queue centrale
static QueueHandle_t gc_queue = NULL;

// Liste des abonnés
typedef struct {
    gc_event_type_t type;
    gc_event_callback_t cb;
} subscriber_t;

static subscriber_t subscribers[GC_MAX_SUBSCRIBERS];
static int subscriber_count = 0;

// ---- Tâche principale ----
static void gc_task(void *pv)
{
    gc_event_t evt;
    while (1) {
        if (xQueueReceive(gc_queue, &evt, portMAX_DELAY) == pdTRUE) {
            // Parcours tous les abonnés pour ce type
            for (int i = 0; i < subscriber_count; i++) {
                if (subscribers[i].type == evt.type && subscribers[i].cb) {
                    subscribers[i].cb(evt);
                }
            }
            ESP_LOGI(TAG, "Event type=%d value=%d", evt.type, evt.value);
        }
    }
}

// ---- Init ----
esp_err_t global_coordinator_init(void)
{
    if (!gc_queue) {
        gc_queue = xQueueCreate(20, sizeof(gc_event_t));
        if (!gc_queue) {
            ESP_LOGE(TAG, "Failed to create queue");
            return ESP_FAIL;
        }
        xTaskCreate(gc_task, "global_coordinator", 4096, NULL, 5, NULL);
        memset(subscribers, 0, sizeof(subscribers));
        subscriber_count = 0;
    }
    return ESP_OK;
}

// ---- Publier un événement ----
esp_err_t global_coordinator_publish(gc_event_type_t type, int value)
{
    if (!gc_queue) return ESP_FAIL;

    gc_event_t evt = { .type = type, .value = value };
    if (xQueueSend(gc_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Queue full, event lost");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// ---- S’abonner ----
esp_err_t global_coordinator_subscribe(gc_event_type_t type, gc_event_callback_t cb)
{
    if (subscriber_count >= GC_MAX_SUBSCRIBERS) {
        ESP_LOGW(TAG, "Max subscribers reached");
        return ESP_ERR_NO_MEM;
    }
    subscribers[subscriber_count].type = type;
    subscribers[subscriber_count].cb = cb;
    subscriber_count++;
    return ESP_OK;
}
