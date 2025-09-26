idf_component_register(SRCS "main.c"
                            "communication_manager.c"
                            "heater_manager.c"
                            "mppt_manager.c"
                            "led_manager.c"
                            "usb_manager.c"
                            "ble_manager.c"
                            "fan_manager.c"
                            "sensor_manager.c"
                            "w5500_ethernet.c"
                            "protocol.c"
                    INCLUDE_DIRS "."
                                "../res"
                    REQUIRES nvs_flash
                            esp_wifi
                            esp_netif
                            esp_http_client
                            esp_eth
                            driver
                            bt
                            esp_bt
                            usb)
