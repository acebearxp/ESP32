idf_component_register(
    SRCS src/IrRC.c src/sony.c src/nec.c
    INCLUDE_DIRS include
    PRIV_REQUIRES driver esp_event)
