
idf_component_register(SRCS app_main.c blink.c task_tm.c
    INCLUDE_DIRS .
    PRIV_REQUIRES onboard extra
        BlinkStatus BlueSetupWiFi WiFiSTA IrRC
        nvs_flash)