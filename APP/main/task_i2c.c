#include "task_i2c.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c.h>
#include <esp_log.h>

#define I2C_RX_BUF_MAX 512

typedef enum _i2c_task_event{
    I2C_TASK_INCOMING
} i2c_task_event_t;

ESP_EVENT_DEFINE_BASE(I2C_TASK_EVENT);

typedef struct _i2c_task_data_{
    esp_event_loop_handle_t hEventLoop;
    esp_event_handler_instance_t hOnEvent;
    char *buf;
    uint16_t u16Pos;
} i2c_task_data_t;

static i2c_task_data_t s_data;

static void task_i2c(void *pArgs)
{
    i2c_task_data_t *pData = (i2c_task_data_t*)pArgs;

    // boosting after the first byte received
    TickType_t tickWait = portMAX_DELAY;
    uint16_t u16Rx = 1;
    while(true){
        int nRead = i2c_slave_read_buffer(I2C_NUM_0, (uint8_t*)pData->buf+pData->u16Pos, u16Rx, tickWait);
        if(nRead > 0){
            pData->u16Pos += nRead;
            u16Rx = I2C_RX_BUF_MAX - 1 - pData->u16Pos;
            tickWait = pdMS_TO_TICKS(100);
        }
        else{
            if(pData->u16Pos > 0){
                pData->buf[pData->u16Pos] = 0;
                u16Rx = 1;
                tickWait = portMAX_DELAY;
                esp_event_post_to(pData->hEventLoop, I2C_TASK_EVENT, I2C_TASK_INCOMING, 0, 0, 0);
            }
        }
    }
    vTaskDelete(NULL);
}

static void on_event(void *pArgs, esp_event_base_t ev, int32_t evid, void *pEvData)
{
    i2c_task_data_t *pdata = (i2c_task_data_t*)pArgs;
    switch(evid){
        case I2C_TASK_INCOMING:
            ESP_LOGI("I2C", "%s", pdata->buf);
            pdata->u16Pos = 0;
        break;
        default:
            ESP_LOGI("I2C", "event: %d", evid);
    }
}

esp_err_t start_i2c(esp_event_loop_handle_t hEventLoop)
{
    s_data.hEventLoop = hEventLoop;
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        hEventLoop, I2C_TASK_EVENT, ESP_EVENT_ANY_ID, on_event, &s_data, &s_data.hOnEvent));
    
    s_data.buf = heap_caps_malloc(I2C_RX_BUF_MAX, MALLOC_CAP_SPIRAM);

    i2c_config_t cfg = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = GPIO_NUM_15,
        .scl_io_num = GPIO_NUM_16,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave = {
            .addr_10bit_en = 0,
            .slave_addr = 0x03,
            .maximum_speed = 400000
        },
        .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL
    };
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_SLAVE, 128, 128, 0));

    xTaskCreate(task_i2c, "i2c_rx", 1024, &s_data, 16, NULL);

    return ESP_OK;
}