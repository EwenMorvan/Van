idf_component_register(SRCS 
                        "main.c"
                        "global_coordinator.c"
                        
                        INCLUDE_DIRS
                        "."

                        REQUIRES 
                        "common_includes"
                        "communications"
                        "peripherals_devices"
                        "utils"
                        
)
