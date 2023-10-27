#include "motor.h"
#include <driver/mcpwm_prelude.h>
#include <esp_log.h>

typedef struct _DRV8837_data{
    mcpwm_timer_handle_t hTimer;
    mcpwm_oper_handle_t hOperator;
    mcpwm_cmpr_handle_t hComparator;
    mcpwm_gen_handle_t hGen1;
    mcpwm_gen_handle_t hGen2;
    uint8_t u8DutyCycle;
    DRV8837_Config_t config;
} DRV8837_data_t;

typedef struct _MotorX4_data{
    DRV8837_data_t wheelLeftFront;
    DRV8837_data_t wheelLeftRear;
    DRV8837_data_t wheelRightFront;
    DRV8837_data_t wheelRightRear;
} MotorX4_data_t;

esp_err_t drv8837_setup(DRV8837_Config_t *pConfig, int nOperGroup, DRV8837_data_t *pData)
{
    pData->config = *pConfig;
    // VCC & SLEEP
    gpio_config_t vs_config = {
        .pin_bit_mask = BIT64(pConfig->pinVCC) | BIT64(pConfig->pinSLEEP),
        .mode = GPIO_MODE_OUTPUT
    };
    ESP_ERROR_CHECK(gpio_config(&vs_config));
    ESP_ERROR_CHECK(gpio_set_drive_capability(pConfig->pinVCC, GPIO_DRIVE_CAP_0)); // ~1mA
    ESP_ERROR_CHECK(gpio_set_drive_capability(pConfig->pinSLEEP, GPIO_DRIVE_CAP_0));
    ESP_ERROR_CHECK(gpio_set_level(pConfig->pinVCC, 1)); // 1.8~7V
    // 初始化成 DriveMode_N 空档滑行状态
    ESP_ERROR_CHECK(gpio_set_level(pConfig->pinSLEEP, 0)); // 0: coast, 1: IN1 & IN2 control

    // PWM for IN1 and IN2
    mcpwm_timer_config_t timConfig = {
        .group_id = nOperGroup,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = 99 // 10MHz/(99+1[0~99]) -> 100kHz PWM
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timConfig, &pData->hTimer));

    mcpwm_operator_config_t operConfig = {
        .group_id = nOperGroup
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operConfig, &pData->hOperator));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(pData->hOperator, pData->hTimer));

    mcpwm_comparator_config_t cmpConfig = {
        .flags.update_cmp_on_tep = true
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(pData->hOperator, &cmpConfig, &pData->hComparator));
    pData->u8DutyCycle = 0;
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(pData->hComparator, pData->u8DutyCycle));

    mcpwm_generator_config_t genConfig = { 0 };
    genConfig.gen_gpio_num = pConfig->pinIN1;
    ESP_ERROR_CHECK(mcpwm_new_generator(pData->hOperator, &genConfig, &pData->hGen1));
    genConfig.gen_gpio_num = pConfig->pinIN2;
    ESP_ERROR_CHECK(mcpwm_new_generator(pData->hOperator, &genConfig, &pData->hGen2));

    // 前半周期高电平,后半周期低电平
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(pData->hGen1, MCPWM_GEN_TIMER_EVENT_ACTION(
        MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(pData->hGen1, MCPWM_GEN_COMPARE_EVENT_ACTION(
        MCPWM_TIMER_DIRECTION_UP, pData->hComparator, MCPWM_GEN_ACTION_LOW)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(pData->hGen2, MCPWM_GEN_TIMER_EVENT_ACTION(
        MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(pData->hGen2, MCPWM_GEN_COMPARE_EVENT_ACTION(
        MCPWM_TIMER_DIRECTION_UP, pData->hComparator, MCPWM_GEN_ACTION_LOW)));
    
    ESP_ERROR_CHECK(mcpwm_timer_enable(pData->hTimer));

    return ESP_OK;
}

void drv8837_teardown(DRV8837_data_t *pData)
{
    ESP_ERROR_CHECK(mcpwm_timer_disable(pData->hTimer));

    ESP_ERROR_CHECK(mcpwm_del_generator(pData->hGen1));
    ESP_ERROR_CHECK(mcpwm_del_generator(pData->hGen2));
    ESP_ERROR_CHECK(mcpwm_del_comparator(pData->hComparator));
    ESP_ERROR_CHECK(mcpwm_del_operator(pData->hOperator));
    ESP_ERROR_CHECK(mcpwm_del_timer(pData->hTimer));
    
    ESP_ERROR_CHECK(gpio_reset_pin(pData->config.pinVCC));
    ESP_ERROR_CHECK(gpio_reset_pin(pData->config.pinSLEEP));
    ESP_ERROR_CHECK(gpio_reset_pin(pData->config.pinIN1));
    ESP_ERROR_CHECK(gpio_reset_pin(pData->config.pinIN2));
}

MotorX4_handler_t MotorX4_driver_install(MotorX4_Config_t *pConfig)
{
    MotorX4_data_t *pMotorX4 = malloc(sizeof(MotorX4_data_t));

    drv8837_setup(&pConfig->motorLeftFront, 0, &pMotorX4->wheelLeftFront);
    drv8837_setup(&pConfig->motorLeftRear, 0, &pMotorX4->wheelLeftRear);
    drv8837_setup(&pConfig->motorRightFront, 1, &pMotorX4->wheelRightFront);
    drv8837_setup(&pConfig->motorRightRear, 1, &pMotorX4->wheelRightRear);

    return pMotorX4;
}

void MotorX4_driver_uninstall(MotorX4_handler_t hMotorX4)
{
    MotorX4_data_t *pData = (MotorX4_data_t*)hMotorX4;
    MotorX4_SwtichDrive(hMotorX4, DriveMode_N);

    drv8837_teardown(&pData->wheelLeftFront);
    drv8837_teardown(&pData->wheelLeftRear);
    drv8837_teardown(&pData->wheelRightFront);
    drv8837_teardown(&pData->wheelRightRear);

    free(pData);
}

// 切换驱动模式
esp_err_t MotorX4_SwtichDrive(MotorX4_handler_t hMotorX4, DriveMode mode)
{
    MotorX4_data_t *pData = (MotorX4_data_t*)hMotorX4;
    DRV8837_data_t wheel[] = {
        pData->wheelLeftFront,
        pData->wheelLeftRear,
        pData->wheelRightFront,
        pData->wheelRightRear
    };

    switch(mode){
        case DriveMode_FWD:
        case DriveMode_RWD:
        {
            int nActived = (mode == DriveMode_FWD) ? 0 : 1;
            for(int i=0;i<4;i++){
                if(i%2 == nActived){
                    // 驱动轮,启动PWM
                    ESP_ERROR_CHECK(mcpwm_timer_start_stop(wheel[i].hTimer, MCPWM_TIMER_START_NO_STOP));
                    // 设置为驱动模式
                    ESP_ERROR_CHECK(gpio_set_level(wheel[i].config.pinSLEEP, 1));
                }
                else{
                    // 从动轮,滑行模式
                    ESP_ERROR_CHECK(gpio_set_level(wheel[i].config.pinSLEEP, 0));
                    // 停止PWM
                    ESP_ERROR_CHECK(mcpwm_timer_start_stop(wheel[i].hTimer, MCPWM_TIMER_STOP_FULL));
                }
            }
            break;
        }
        case DriveMode_4WD:
        {
            for(int i=0;i<4;i++){
                // 启动PWM
                ESP_ERROR_CHECK(mcpwm_timer_start_stop(wheel[i].hTimer, MCPWM_TIMER_START_NO_STOP));
                // 设置为驱动模式
                ESP_ERROR_CHECK(gpio_set_level(wheel[i].config.pinSLEEP, 1));
            }
            break;
        }
        case DriveMode_N:
        default:
        {
            for(int i=0;i<4;i++){
                // 设置为滑行模式
                ESP_ERROR_CHECK(gpio_set_level(wheel[i].config.pinSLEEP, 0));
                // 停止PWM
                ESP_ERROR_CHECK(mcpwm_timer_start_stop(wheel[i].hTimer, MCPWM_TIMER_STOP_FULL));
            }
        }
    }
    
    return ESP_OK;
}

// 行驶 n8Speed: -100 ~ +100 负数表示倒退
esp_err_t MotorX4_Drive(MotorX4_handler_t hMotorX4, int8_t n8Speed)
{
    MotorX4_data_t *pData = (MotorX4_data_t*)hMotorX4;
    DRV8837_data_t wheel[] = {
        pData->wheelLeftFront,
        pData->wheelLeftRear,
        pData->wheelRightFront,
        pData->wheelRightRear
    };

    if(n8Speed > 99) n8Speed = 99;
    else if(n8Speed < -99) n8Speed = -99;
    
    // 为避免突变冲击,先切到滑行模式,再设定PWM
    for(int i=0;i<4;i++){
        if(n8Speed >= 0){
            wheel[i].u8DutyCycle = n8Speed;
            ESP_ERROR_CHECK(mcpwm_generator_set_force_level(wheel[i].hGen1, 0, true));
            ESP_ERROR_CHECK(mcpwm_generator_set_force_level(wheel[i].hGen2, 0, true));
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(wheel[i].hComparator, wheel[i].u8DutyCycle));
            ESP_ERROR_CHECK(mcpwm_generator_set_force_level(wheel[i].hGen1, -1, true));
        }
        else{
            wheel[i].u8DutyCycle = -n8Speed;
            ESP_ERROR_CHECK(mcpwm_generator_set_force_level(wheel[i].hGen1, 0, true));
            ESP_ERROR_CHECK(mcpwm_generator_set_force_level(wheel[i].hGen2, 0, true));
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(wheel[i].hComparator, wheel[i].u8DutyCycle));
            ESP_ERROR_CHECK(mcpwm_generator_set_force_level(wheel[i].hGen2, -1, true));
        }
    }
    
    return ESP_OK;
}

// 左转 n8Ratio: -100 ~ +100 左轮相对右轮的速度比例
// 90代表轻微左转 50代表急转弯 0代表左轮不动 负数表示左轮反向转动(更急的左转)
esp_err_t MotorX4_TurnLeft(MotorX4_handler_t hMotorX4, int8_t n8Ratio)
{
    return ESP_FAIL;
}

// 右转 n8Ratio: -100 ~ +100 右轮相对左轮的速度比例
// 90代表轻微右转 50代表急转弯 0代表右轮不动 负数表示右轮反向转动(更急的右转)
esp_err_t MotorX4_TurnRight(MotorX4_handler_t hMotorX4, int8_t n8Ratio)
{
    return ESP_FAIL;
}

// 刹车
esp_err_t MotorX4_Break(MotorX4_handler_t hMotorX4)
{
    MotorX4_data_t *pData = (MotorX4_data_t*)hMotorX4;
    DRV8837_data_t wheel[] = {
        pData->wheelLeftFront,
        pData->wheelLeftRear,
        pData->wheelRightFront,
        pData->wheelRightRear
    };

    for(int i=0;i<4;i++){
        wheel[i].u8DutyCycle = 0;
        ESP_ERROR_CHECK(mcpwm_generator_set_force_level(wheel[i].hGen1, 1, true));
        ESP_ERROR_CHECK(mcpwm_generator_set_force_level(wheel[i].hGen2, 1, true));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(wheel[i].hComparator, wheel[i].u8DutyCycle));
    }

    return ESP_OK;
}
