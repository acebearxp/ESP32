set(include)

idf_component_register(
    SRCS src/BlueSetupWiFi.c src/GAP_NimBLE.c src/GATT_NimBLE.c
    INCLUDE_DIRS include
    PRIV_REQUIRES nvs_flash bt)
