idf_component_register(SRCS "main.c"
                            "error_manager.c"
                            "shift_register.c"
                            "communication_manager.c"
                            "electrovalve_pump_manager.c"
                            "button_manager.c"
                            "load_cell_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES freertos driver esp_timer nvs_flash esp_netif esp_eth)