#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <esp_log.h>
#include <string.h>
#include "IrRC.h"
#include "sony.h"
#include "nec.h"

#define RMT_RX_BUF_SIZE 512

typedef struct _IrRC_data_struct{
    esp_event_loop_handle_t hEventLoop;
    esp_event_handler_instance_t hOnEvent;
    gpio_num_t gpio;
    rmt_channel_handle_t hRx;
    rmt_symbol_word_t bufRx[RMT_RX_BUF_SIZE];
    size_t sizeRx;
    IrRC_OnData onData;
    void *pOnDataArgs;
    struct _IrRC_data_struct *pNext;
} _IrRC_data_t;

static const char c_szTAG[] = "IrRC";
static _IrRC_data_t *s_pData;

typedef enum _ir_rc_event{
    IR_RC_EVENT_RX
} ir_rc_event_t;

ESP_EVENT_DEFINE_BASE(IR_RC_EVENT);

static _IrRC_data_t* _search(gpio_num_t gpio)
{
    _IrRC_data_t *pData = s_pData;
    while(pData){
        if(pData->gpio == gpio) break;
        pData = pData->pNext;
    }
    return pData;
}

static _IrRC_data_t* _create()
{
    _IrRC_data_t *pNew = malloc(sizeof(_IrRC_data_t));
    pNew->pNext = NULL;

    if(s_pData == NULL){
        s_pData = pNew;
    }
    else{
        _IrRC_data_t *pData = s_pData;
        while(pData->pNext) pData = pData->pNext;
        pData->pNext = pNew;
    }

    return pNew;
}

static void _destroy(_IrRC_data_t *pData)
{
    if(s_pData == pData){
        s_pData = pData->pNext;
    }
    else{
        _IrRC_data_t *pBefore = s_pData;
        while(pBefore->pNext && pBefore->pNext != pData) pBefore = pBefore->pNext;
        assert(pBefore->pNext == pData);
        pBefore->pNext = pData->pNext;
    }

    free(pData);
}

static void _ir_decode(rmt_symbol_word_t *pRC, size_t len, _IrRC_data_t *pData)
{
    if(pData->onData){
        if(is_sony_protocol(pRC, len)){
            uint8_t u8data[3];
            if(sony_decode(pRC, len, &u8data[0], &u8data[1], &u8data[2])){
                pData->onData(pData->gpio, (u8data[2]<<8)|u8data[1], u8data[0], pData->pOnDataArgs);
            }
        }
        else if(is_nec_protocol(pRC, len)){
            uint8_t u8Code;
            uint16_t u16Addr;
            if(nec_decode(pRC, len, &u8Code, &u16Addr)){
                pData->onData(pData->gpio, u16Addr, u8Code, pData->pOnDataArgs);
            }
        }
        else{
            ESP_LOGW(c_szTAG, "==unsupported ir protocol==>%u", len);
        }
    }
}

static void on_event(void *pArgs, esp_event_base_t ev, int32_t evid, void *pRC)
{
    _IrRC_data_t *pData = (_IrRC_data_t*)pArgs;

    switch(evid){
        case IR_RC_EVENT_RX:
        {
            if(pData->sizeRx > 0){
                /*ESP_LOGI(c_szTAG, "on_event recv: %u", pData->sizeRx);
                for(int i=0;i<pData->sizeRx;i++){
                    ESP_LOGI(c_szTAG, "%u -> %u %u | %u %u", i,
                        pData->bufRx[i].level0, pData->bufRx[i].duration0,
                        pData->bufRx[i].level1, pData->bufRx[i].duration1);
                }*/

                // rmt_symbol_word_t有4个字节
                _ir_decode(pData->bufRx, pData->sizeRx*4, pData);
                pData->sizeRx = 0;
            }
            break;
        }
        default:
            break;
    }

    // 最小信号562.5us,最大信号9ms
    rmt_receive_config_t recv_config = {
        .signal_range_min_ns = 1250, // 12.5us
        .signal_range_max_ns = 12 * 1000 * 1000 // 12ms
    };
    // non-blocking
    rmt_receive(pData->hRx, pData->bufRx, RMT_RX_BUF_SIZE, &recv_config);
}

static bool IRAM_ATTR rmt_rx_callback(rmt_channel_handle_t hRX, const rmt_rx_done_event_data_t *pDataRecv, void *pEvData)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    _IrRC_data_t *pData = (_IrRC_data_t*)pEvData;
    pData->sizeRx = pDataRecv->num_symbols;
    esp_event_isr_post_to(pData->hEventLoop, IR_RC_EVENT, IR_RC_EVENT_RX, NULL, 0, &xHigherPriorityTaskWoken);

    return xHigherPriorityTaskWoken == pdTRUE;
}

esp_err_t IrRC_Init(esp_event_loop_handle_t hEventLoop, gpio_num_t gpio)
{
    _IrRC_data_t *pData = _search(gpio);
    assert(pData == NULL);
    pData = _create();

    pData->hEventLoop = hEventLoop;
    pData->gpio = gpio;
    pData->hRx = NULL;
    pData->sizeRx = 0;
    pData->onData = NULL;
    pData->pOnDataArgs = NULL;

    if(hEventLoop){
        ESP_ERROR_CHECK(esp_event_handler_instance_register_with(hEventLoop, IR_RC_EVENT, ESP_EVENT_ANY_ID, on_event, pData, &pData->hOnEvent));
    }
    
    rmt_rx_channel_config_t config = {
        .gpio_num = gpio,
        .clk_src = RMT_CLK_SRC_APB, // APB@80MHz
        .resolution_hz = 1*1000*1000, // 1MHz
        .mem_block_symbols = 128,
        .flags.with_dma = 0
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&config, &pData->hRx));

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = rmt_rx_callback
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(pData->hRx, &cbs, pData));
    ESP_ERROR_CHECK(rmt_enable(pData->hRx));

    // trigger rmt_receive
    return esp_event_post_to(pData->hEventLoop, IR_RC_EVENT, IR_RC_EVENT_RX, NULL, 0, 0);
}

esp_err_t IrRC_Deinit(gpio_num_t gpio)
{
    _IrRC_data_t *pData = _search(gpio);
    assert(pData != NULL);

    ESP_ERROR_CHECK(rmt_disable(pData->hRx));
    ESP_ERROR_CHECK(rmt_del_channel(pData->hRx));

    if(pData->hEventLoop){
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister_with(pData->hEventLoop, IR_RC_EVENT, ESP_EVENT_ANY_ID, pData->hOnEvent));
    }

    _destroy(pData);
    return ESP_OK;
}

esp_err_t IrRC_Set_OnData(gpio_num_t gpio, IrRC_OnData ondata, void *pArgs)
{
    _IrRC_data_t *pData = _search(gpio);
    assert(pData != NULL);
    pData->onData = ondata;
    pData->pOnDataArgs = pArgs;
    return ESP_OK;
}