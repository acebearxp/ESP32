// 驱动一台遥控小车上4个由DRV8837驱动的有刷电机连接的车轮
#include <driver/gpio.h>
#include <esp_err.h>

typedef enum _DriveMode {
    DriveMode_N,    // 空档,四轮处于滑行状态
    DriveMode_FWD,  // 前驱模式,后轮被动滑行
    DriveMode_RWD,  // 后驱模式,前轮被动滑行
    DriveMode_4WD   // 四驱模式
} DriveMode;

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

// 切换驱动模式
esp_err_t MotorX4_SwtichDrive(MotorX4_handler_t hMotorX4, DriveMode mode);
// 行驶 n8Speed: -100 ~ +100 负数表示倒退
esp_err_t MotorX4_Drive(MotorX4_handler_t hMotorX4, int8_t n8Speed);
// 左转 n8Ratio: -100 ~ +100 左轮相对右轮的速度比例 90代表轻微左转 50代表急转弯 0代表左轮不动 负数表示左轮反向转动(更急的左转)
esp_err_t MotorX4_TurnLeft(MotorX4_handler_t hMotorX4, int8_t n8Ratio);
// 右转 n8Ratio: -100 ~ +100 右轮相对左轮的速度比例 90代表轻微右转 50代表急转弯 0代表右轮不动 负数表示右轮反向转动(更急的右转)
esp_err_t MotorX4_TurnRight(MotorX4_handler_t hMotorX4, int8_t n8Ratio);
// 刹车
esp_err_t MotorX4_Break(MotorX4_handler_t hMotorX4);