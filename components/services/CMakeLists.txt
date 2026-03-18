idf_component_register(
    SRCS "led_service.c"
         "beep_service.c"
         "button_service.c"
         "display_service.c"
         "wifi_service.c"
         "http_service.c"
    INCLUDE_DIRS "."
    REQUIRES
        bsp
        freertos
        log
        system
        esp_timer
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        esp_http_client
        json
)
