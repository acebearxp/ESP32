#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <esp_log.h>
#include <string.h>
#include "IrRC.h"
#include "sony.h"
#include "nec.h"

typedef struct _IrRC_data_struct{
    esp_event_loop_handle_t hEventLoop;
    esp_event_handler_instance_t hOnEvent;
    gpio_num_t gpio;
    rmt_channel_t chRx;
    RingbufHandle_t buf;
    TaskHandle_t taskRx;
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

static void _ir_decode(rmt_item32_t *pRC, size_t len, _IrRC_data_t *pData)
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
            _ir_decode(pRC, pData->sizeRx, pData);
            pData->sizeRx = 0;
            break;
        }
        default:
            break;
    }
}

static void _task_rx_data(void *pArgs)
{
    _IrRC_data_t *pData = (_IrRC_data_t*)pArgs;
    size_t len;
    rmt_item32_t *pRC;

    while(true){
        pRC = (rmt_item32_t*)xRingbufferReceive(pData->buf, &len, portMAX_DELAY);
        if(pRC){
            if(pData->hEventLoop && pData->sizeRx == 0){
                pData->sizeRx = len;
                esp_event_post_to(pData->hEventLoop, IR_RC_EVENT, IR_RC_EVENT_RX, pRC, len, 0);
            }
            else{
                _ir_decode(pRC, len, pData);
            }
            vRingbufferReturnItem(pData->buf, pRC);
        }
    }
}

esp_err_t IrRC_Init(esp_event_loop_handle_t hEventLoop, gpio_num_t gpio, rmt_channel_t chRx, UBaseType_t uxPriority)
{
    _IrRC_data_t *pData = _search(gpio);
    assert(pData == NULL);
    pData = _create();

    pData->hEventLoop = hEventLoop;
    pData->gpio = gpio;
    pData->chRx = chRx;
    pData->onData = NULL;
    pData->pOnDataArgs = NULL;

    if(hEventLoop){
        ESP_ERROR_CHECK(esp_event_handler_instance_register_with(hEventLoop, IR_RC_EVENT, ESP_EVENT_ANY_ID, on_event, pData, &pData->hOnEvent));
    }

    rmt_config_t config = RMT_DEFAULT_CONFIG_RX(gpio, chRx);
    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(chRx, 512, 0));

    uint32_t u32Hz;
    rmt_get_counter_clock(chRx, &u32Hz);
    ESP_LOGI(c_szTAG, "RMT %u in %uHz", chRx, u32Hz);

    // rmt_rx
    ESP_ERROR_CHECK(rmt_get_ringbuf_handle(chRx, &pData->buf));
    BaseType_t xRet = xTaskCreate(_task_rx_data, c_szTAG, 3072, pData, uxPriority, &pData->taskRx);
    assert(xRet == pdPASS);

    return rmt_rx_start(chRx, true);
}

esp_err_t IrRC_Deinit(gpio_num_t gpio)
{
    _IrRC_data_t *pData = _search(gpio);
    assert(pData != NULL);

    ESP_ERROR_CHECK(rmt_rx_stop(pData->chRx));
    vTaskDelete(pData->taskRx);
    ESP_ERROR_CHECK(rmt_driver_uninstall(pData->chRx));

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