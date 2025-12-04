idf_component_register(
    SRCS 
        "button_executor.c"
        "electrovalve_executor.c"
        "pump_executor.c"
        "buttons_manager.c"
        "electrovalves_pumps_manager.c"
        "load_cells_manager.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        "io_drivers"
        "common_includes"
        "freertos"
        "driver"
        "esp_timer"
       
)