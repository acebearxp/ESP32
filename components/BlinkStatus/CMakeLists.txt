# idf_build_get_property(components_to_build BUILD_COMPONENTS)

set(include)

idf_component_register(
    SRCS src/BlinkStatus.c
    INCLUDE_DIRS include
    PRIV_REQUIRES driver esp_common SK68xx)
