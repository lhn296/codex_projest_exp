idf_component_register(
    SRCS "bsp_led.c"
         "bsp_button.c"
         "bsp_xl9555.c"
         "bsp_lcd.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        driver
        freertos
        log
        system
)
