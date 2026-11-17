idf_component_register(SRCS "pump_task.c"
                       INCLUDE_DIRS "include"
                       REQUIRES events
                       PRIV_REQUIRES relay esp_timer)