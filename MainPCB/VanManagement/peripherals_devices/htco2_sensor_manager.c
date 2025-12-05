#include "htco2_sensor_manager.h"

static const char *TAG = "HTCO2_MGR";

static TaskHandle_t htco2_task_handle = NULL;

htco2_sensor_t htco2_data = {0};

// Buffer to accumulate a line
static char line_buf[256];
static size_t line_pos = 0;

static void htco2_task(void *pv) {
    (void) pv;
    ESP_LOGI(TAG, "HCO2T task started");

    while (1) {
        // Ensure UART2 is switched to HCO2T
        esp_err_t err = uart_mux_switch_sensor(SENSOR_HCO2T);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to switch UART2 to HCO2T: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Ensure baud rate is 115200 for this sensor
        uart_set_baudrate(UART_NUM_2, 115200);

        // Read available bytes (short timeout so we don't block too long)
        uint8_t buf[128];
        int read = uart_mux_read_sensor(buf, sizeof(buf), 200);
        if (read > 0) {
            for (int i = 0; i < read; ++i) {
                char c = (char)buf[i];
                if (c == '\r') continue; // ignore CR
                if (c == '\n' || line_pos >= (sizeof(line_buf)-1)) {
                    // terminate and parse
                    line_buf[line_pos] = '\0';
                    if (line_pos > 0) {
                        // Expected format: CO2,Temp_tenths,Hum_tenths,Light
                        uint32_t co2 = 0;
                        int32_t t_tenths = 0;
                        int32_t h_tenths = 0;
                        int32_t light = 0;

                        int parsed = sscanf(line_buf, "%" SCNu32 ",%" SCNd32 ",%" SCNd32 ",%" SCNd32, &co2, &t_tenths, &h_tenths, &light);
                        if (parsed >= 3) {
                            htco2_data.co2 = co2;
                            htco2_data.t_tenths = t_tenths;
                            htco2_data.h_tenths = h_tenths;
                            htco2_data.light = light;
                            ESP_LOGD(TAG, "Parsed HCO2T: CO2=%" PRIu32 " ppm, T=%.1f°C, RH=%.1f%%, light=%" PRId32, co2, ((float)t_tenths) / 10.0f, ((float)h_tenths) / 10.0f, light);
                            
                        } else {
                            ESP_LOGW(TAG, "Failed to parse HCO2T line: '%s' (parsed=%d)", line_buf, parsed);
                        }
                    }
                    // reset buffer
                    line_pos = 0;
                } else {
                    line_buf[line_pos++] = c;
                }
            }
        } else if (read == -1) {
            ESP_LOGW(TAG, "uart_mux_read_sensor timeout or error");
        }

        // Wait 5 seconds between polls
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t htco2_sensor_manager_init(void) {
    if (htco2_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE; // already running
    }

    // Create task
    BaseType_t r = xTaskCreate(htco2_task, "htco2_task", 4096, NULL, tskIDLE_PRIORITY + 5, &htco2_task_handle);
    if (r != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HCO2T task");
        htco2_task_handle = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "HCO2T sensor manager initialized");
    return ESP_OK;
}

esp_err_t htco2_sensor_manager_deinit(void) {
    if (htco2_task_handle) {
        vTaskDelete(htco2_task_handle);
        htco2_task_handle = NULL;
    }
    return ESP_OK;
}

esp_err_t htco2_sensor_manager_update_van_state(van_state_t* van_state) {

    van_state->sensors.co2_level = (uint16_t)htco2_data.co2;
    van_state->sensors.cabin_temperature = ((float)htco2_data.t_tenths) / 10.0f;
    van_state->sensors.humidity = ((float)htco2_data.h_tenths) / 10.0f;
    van_state->sensors.light = (uint16_t)htco2_data.light;
    
    return ESP_OK;
}
