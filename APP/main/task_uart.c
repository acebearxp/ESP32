#include "task_uart.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_log.h>

typedef enum _uart_task_event{
    UART_TASK_INCOMING,
    UART_TASK_IR
} uart_task_event_t;

ESP_EVENT_DEFINE_BASE(UART_TASK_EVENT);

#define UART_RX_BUF_MAX 512

typedef struct _uart_task_data_{
    esp_event_loop_handle_t hEventLoop;
    esp_event_handler_instance_t hOnEvent;
    QueueHandle_t hQueueUART;
    IrRC_OnData on_ir_data;
    char *buf;
} uart_task_data_t;

static uart_task_data_t s_data;

static void process_x12_cmd(char *buf, size_t len, uart_task_data_t *pdata)
{
    for(size_t i=0;i<len;i++){
        if(i>0 && buf[i-1] == 0x12){
            int pos_end = i + (uint8_t)buf[i] - 2;
            if(pos_end < len && buf[pos_end] == 0x12){
                if((uint8_t)buf[i] == 6){
                    esp_event_post_to(pdata->hEventLoop, UART_TASK_EVENT, UART_TASK_IR, buf+i+1, 3, 0);
                }
                for(int j=i-1;j<=pos_end;j++) buf[j] = 0x23;
                i = pos_end;
            }
        }
    }
}

static void on_event(void *pArgs, esp_event_base_t ev, int32_t evid, void *pEvData)
{
    uart_task_data_t *pdata = (uart_task_data_t*)pArgs;
    switch(evid){
        case UART_TASK_IR:
        {
            char *buf = (char*)pEvData;
            uint16_t u16Addr = (buf[0]<<8) | buf[1];
            uint8_t u8Code = buf[2];
            pdata->on_ir_data(GPIO_NUM_NC, u16Addr, u8Code, NULL);
            break;
        }
        default:
        {
            uart_event_t *pEventUART = (uart_event_t*)pEvData;
            switch(pEventUART->type){
                case UART_DATA:
                {
                    size_t len = pEventUART->size;
                    while(len > 0){
                        len = uart_read_bytes(UART_NUM_0, pdata->buf, UART_RX_BUF_MAX-1, pdMS_TO_TICKS(100));
                        // rm mid 0
                        for(int i=0;i<len;i++){
                            if(pdata->buf[i] == 0) pdata->buf[i] = '.';
                        }
                        pdata->buf[len] = 0;

                        // X12 Cmd
                        process_x12_cmd(pdata->buf, len, pdata);
                        ESP_LOGI("UART", "%s", pdata->buf);
                        uart_get_buffered_data_len(UART_NUM_0, &len);
                    }
                    break;
                }
                default:
                    ESP_LOGI("UART", "event: %d", pEventUART->type);
            }
        }
    }
}

static void task_uart(void *pArgs)
{
    uart_task_data_t *pdata = (uart_task_data_t*)pArgs;

    uart_event_t evUART;
    while(xQueueReceive(pdata->hQueueUART, &evUART, portMAX_DELAY)){
        esp_event_post_to(pdata->hEventLoop, UART_TASK_EVENT, UART_TASK_INCOMING, &evUART, sizeof(uart_event_t), 0);
    }
}

esp_err_t start_uart(esp_event_loop_handle_t hEventLoop, IrRC_OnData on_ir_data)
{
    s_data.hEventLoop = hEventLoop;
    s_data.on_ir_data = on_ir_data;
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        hEventLoop, UART_TASK_EVENT, ESP_EVENT_ANY_ID, on_event, &s_data, &s_data.hOnEvent));

    s_data.buf = heap_caps_malloc(UART_RX_BUF_MAX, MALLOC_CAP_SPIRAM);

    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 256, 16, &s_data.hQueueUART, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, GPIO_NUM_17, GPIO_NUM_18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_rts(UART_NUM_0, true));
    ESP_ERROR_CHECK(uart_set_dtr(UART_NUM_0, true));
    ESP_ERROR_CHECK(uart_set_mode(UART_NUM_0, UART_MODE_UART));

    assert(xTaskCreate(task_uart, "uart_rx", 1024, &s_data, 16, 0) == pdPASS);

    return ESP_OK;
}