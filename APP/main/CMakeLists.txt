
idf_component_register(SRCS app_main.c blink.c motor.c task_tm.c task_uart.c task_i2c.c
    INCLUDE_DIRS .
    PRIV_REQUIRES onboard extra
        BlinkStatus BlueSetupWiFi WiFiSTA IrRC
        nvs_flash)