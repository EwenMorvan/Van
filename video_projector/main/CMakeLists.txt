idf_component_register(SRCS "ble_manager.c" "ir_led_manager.c" "usb_manager.c" "button_manager.c" "motor_manager.c" "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES freertos nvs_flash esp_timer driver bt esp_system )
