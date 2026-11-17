idf_component_register(SRCS "app_main.c" "bringup.c"
                    INCLUDE_DIRS "."
                    REQUIRES board events relay buttons pump esp_driver_gpio)
