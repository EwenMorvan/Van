#pragma once
#include "led_manager.h"
#include "led_strip.h"
#include "esp_err.h"

esp_err_t led_static_init_strips(led_strip_handle_t strips[]);
void led_static_off(led_strip_t strip, uint8_t brightness);
void led_static_white(led_strip_t strip, uint8_t brightness);
void led_static_orange(led_strip_t strip, uint8_t brightness);
void led_static_film(led_strip_t strip, uint8_t brightness);
