idf_component_register(SRCS "buttons.c"
                       INCLUDE_DIRS "include"
                       REQUIRES board events esp_driver_gpio esp_timer)