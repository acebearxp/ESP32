#include "motor.h"
#include <driver/mcpwm_prelude.h>
#include <esp_log.h>

typedef struct _MotorX4_data{
    mcpwm_timer_handle_t timLeft;
    mcpwm_oper_handle_t operLeft;
    mcpwm_cmpr_handle_t cmpLeft;
    mcpwm_gen_handle_t genLeft1;
    uint8_t u8Left1Duty;
    
    mcpwm_timer_handle_t timRight;
    mcpwm_oper_handle_t operRight;
    // TODO: ...
} MotorX4_data_t;

MotorX4_handler_t MotorX4_driver_install(MotorX4_Config_t *pConfig)
{
    MotorX4_data_t *pData = malloc(sizeof(MotorX4_data_t));
    pData->u8Left1Duty = 49;

    mcpwm_timer_config_t timConfig = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = 99 // 10MHz/(99+1[0~99]) = 100kHz
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timConfig, &pData->timLeft));

    mcpwm_operator_config_t operConfig = {
        .group_id = 0
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operConfig, &pData->operLeft));

    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(pData->operLeft, pData->timLeft));

    mcpwm_comparator_config_t cmpConfig = {
        .flags.update_cmp_on_tez = true
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(pData->operLeft, &cmpConfig, &pData->cmpLeft));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(pData->cmpLeft, pData->u8Left1Duty));

    mcpwm_generator_config_t genConfig = {
        .gen_gpio_num = pConfig->motorLeftFront.pinIN1,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(pData->operLeft, &genConfig, &pData->genLeft1));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(pData->genLeft1, MCPWM_GEN_TIMER_EVENT_ACTION(
        MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(pData->genLeft1, MCPWM_GEN_COMPARE_EVENT_ACTION(
        MCPWM_TIMER_DIRECTION_UP, pData->cmpLeft, MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_timer_enable(pData->timLeft));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(pData->timLeft, MCPWM_TIMER_START_NO_STOP));

    return pData;
}

void MotorX4_driver_uninstall(MotorX4_handler_t hMotorX4)
{
    MotorX4_data_t *pData = (MotorX4_data_t*)hMotorX4;

    ESP_ERROR_CHECK(mcpwm_timer_start_stop(pData->timLeft, MCPWM_TIMER_STOP_FULL));
    ESP_ERROR_CHECK(mcpwm_timer_disable(pData->timLeft));
    ESP_ERROR_CHECK(mcpwm_del_generator(pData->genLeft1));
    ESP_ERROR_CHECK(mcpwm_del_comparator(pData->cmpLeft));
    ESP_ERROR_CHECK(mcpwm_del_operator(pData->operLeft));
    ESP_ERROR_CHECK(mcpwm_del_timer(pData->timLeft));

    free(pData);
}

esp_err_t MotorX4_inc(MotorX4_handler_t hMotorX4)
{
    MotorX4_data_t *pData = (MotorX4_data_t*)hMotorX4;
    pData->u8Left1Duty+=5;
    if(pData->u8Left1Duty > 99){
        pData->u8Left1Duty = 99;
    }
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(pData->cmpLeft, pData->u8Left1Duty));
    ESP_LOGI("MotorX4", "u8Left1Duty: %u", pData->u8Left1Duty);

    return ESP_OK;
}

esp_err_t MotorX4_dec(MotorX4_handler_t hMotorX4)
{
    MotorX4_data_t *pData = (MotorX4_data_t*)hMotorX4;
    pData->u8Left1Duty-=5;
    if(pData->u8Left1Duty > 99){
        pData->u8Left1Duty = 0;
    }
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(pData->cmpLeft, pData->u8Left1Duty));
    ESP_LOGI("MotorX4", "u8Left1Duty: %u", pData->u8Left1Duty);

    return ESP_OK;
}