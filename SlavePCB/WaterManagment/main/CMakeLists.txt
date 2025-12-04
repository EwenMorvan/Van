# idf_component_register(SRCS  
#                         # "Communications/Ethernet/ethernet.c"
#                         #     "wifi_ota_manager.c" "main.c"
#                         #     "error_manager.c"
#                         #     "shift_register.c"
#                         #     "communication_manager.c"
#                         #     "electrovalve_pump_manager.c"
#                         #     "button_manager.c"
#                         #     "load_cell_manager_simple.c"
#                         #     "w5500_comm.c"
#                         "main3.c"
#                     INCLUDE_DIRS "."
#                     REQUIRES IO_drivers freertos driver esp_timer nvs_flash esp_netif lwip esp_eth bt esp_wifi esp_event esp_http_server app_update esp_http_client json)   

idf_component_register(
    SRCS 
        "cases_manager.c"
        "main3.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        "io_drivers"
        "communications"
        "peripherals_devices"
)