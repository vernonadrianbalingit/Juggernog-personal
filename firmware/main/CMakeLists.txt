idf_component_register(SRCS "app_main.c" "bringup.c" "boot_stats.c"
                       INCLUDE_DIRS "."
                       REQUIRES board events relay buttons pump hcsr04 pn532 esp_driver_gpio)
