idf_component_register(
    SRCS "bsp_led.c"
         "bsp_button.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        log
        system
)
