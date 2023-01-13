idf_component_register(
    SRCS button.c
    INCLUDE_DIRS .
    REQUIRES BlueSetupWiFi driver esp_event esp_timer extra)