idf_component_register(SRCS "pn532.c" "pn532_frame.c" "nfc_task.c"
                       INCLUDE_DIRS "include"
                       REQUIRES events
                       PRIV_REQUIRES board esp_driver_i2c esp_timer)