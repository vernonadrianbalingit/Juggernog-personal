idf_component_register(SRCS "hcsr04.c" "cup_filter.c" "sensor_task.c"
                       INCLUDE_DIRS "include"
                       REQUIRES events
                       PRIV_REQUIRES board esp_driver_gpio esp_timer esp_rom)