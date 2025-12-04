#pragma once
#include "led_manager.h"
#include "led_strip.h"
#include "esp_err.h"

esp_err_t led_dynamic_rainbow(led_strip_t strip, uint8_t brightness);
esp_err_t led_dynamic_door_open(led_strip_t strip, uint8_t brightness, bool direction);
void led_dynamic_stop(led_strip_t strip);
