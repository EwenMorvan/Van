#include "fan_manager.h"
#include "esp_log.h"
#include "driver/ledc.h"

static const char *TAG = "FAN_MGR";

static uint8_t current_fan_speed = 0; // Stockage de la vitesse actuelle du ventilateur

esp_err_t fan_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing fan manager...");

    // GPIO and PWM configuration for fan control
    // Configure LEDC timer for PWM (25 kHz, 8-bit resolution)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .timer_num        = LEDC_TIMER_1,
        .freq_hz          = 25000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(err));
        return err;
    }

    // Configure LEDC channel mapped to FAN_HEATER_PWM pin
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_1,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = FAN_HEATER_PWM,
        .duty       = 0, // start with 0% duty
        .hpoint     = 0
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(err));
        return err;
    }

    // Initialisation du matériel de gestion du ventilateur ici
    return ESP_OK;
}

esp_err_t fan_manager_set_speed(uint8_t speed_percent)
{
    if (speed_percent > 100) {
        speed_percent = 100;
    }

    ESP_LOGI(TAG, "Setting fan speed to %d%%", speed_percent);

    // Calculer le duty cycle pour PWM 8-bit (0-255)
    uint32_t duty = (speed_percent * 255) / 100;

    // Définir le duty cycle
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LEDC duty: %s", esp_err_to_name(err));
        return err;
    }

    // Mettre à jour le duty cycle
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update LEDC duty: %s", esp_err_to_name(err));
        return err;
    }

    // Stocker la vitesse actuelle
    current_fan_speed = speed_percent;

    return ESP_OK;
}

uint8_t fan_manager_get_speed(void)
{
    // Retourner la vitesse actuelle définie (pas de capteur de tachymètre)
    return current_fan_speed;
}
