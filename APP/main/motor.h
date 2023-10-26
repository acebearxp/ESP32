// 驱动一台遥控小车上4个由DRV8837驱动的有刷电机连接的车轮
#include <driver/gpio.h>
#include <esp_err.h>

typedef struct _DRV8837_Config{
    gpio_num_t pinVCC;
    gpio_num_t pinSLEEP;
    gpio_num_t pinIN1;
    gpio_num_t pinIN2;
} DRV8837_Config_t;

typedef struct MotorX4_Config{
    DRV8837_Config_t motorLeftFront;
    DRV8837_Config_t motorLeftRear;
    DRV8837_Config_t motorRightFront;
    DRV8837_Config_t motorRightRear;
} MotorX4_Config_t;

typedef void* MotorX4_handler_t;

MotorX4_handler_t MotorX4_driver_install(MotorX4_Config_t *pConfig);
void MotorX4_driver_uninstall(MotorX4_handler_t hMotorX4);

esp_err_t MotorX4_inc(MotorX4_handler_t hMotorX4);
esp_err_t MotorX4_dec(MotorX4_handler_t hMotorX4);