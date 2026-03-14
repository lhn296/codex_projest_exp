idf_component_register(
    SRCS "led_service.c"
         "button_service.c"
    INCLUDE_DIRS "."
    REQUIRES
        bsp
        freertos
        log
        system
        esp_timer
)
