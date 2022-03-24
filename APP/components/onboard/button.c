#include "button.h"
#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include "BlueSetupWiFi.h"


#define MAX_ACTIVE_SECONDS 30

typedef enum _onboard_event{
    ONBOARD_EVENT_BTN_ACTIVE_BLE,
    ONBOARD_EVENT_TICK
} onboard_event_t;

ESP_EVENT_DEFINE_BASE(ONBOARD_EVENT);

typedef struct _onboard_data{
    esp_event_loop_handle_t hEventLoop;
    esp_event_handler_instance_t hOnEvent;
    esp_timer_handle_t hTimer;
    bool bActiveBLE;
    uint8_t u8CountDeactiveBLE;
} onboard_data_t;

static const char c_szName[] = "AceBear-ESP32S3-DevKitC-1";
static onboard_data_t s_data;

static void IRAM_ATTR gpio_isr_handler(void *pArgs)
{
    onboard_data_t *pOnboardData = (onboard_data_t*)pArgs;
    esp_event_isr_post_to(pOnboardData->hEventLoop, ONBOARD_EVENT, ONBOARD_EVENT_BTN_ACTIVE_BLE, NULL, 0, 0);
}

static void on_event(void *pArgs, esp_event_base_t ev, int32_t evid, void *pData)
{
    onboard_data_t *pOnboardData = (onboard_data_t*)pArgs;

    switch (evid)
    {
    case ONBOARD_EVENT_BTN_ACTIVE_BLE:
        pOnboardData->u8CountDeactiveBLE = MAX_ACTIVE_SECONDS;
        if(!pOnboardData->bActiveBLE){
            StartBleAdv();
            pOnboardData->bActiveBLE = true;
            esp_timer_start_periodic(pOnboardData->hTimer, 1000000);
        }
        break;
    case ONBOARD_EVENT_TICK:
        if(pOnboardData->u8CountDeactiveBLE > 0) pOnboardData->u8CountDeactiveBLE--;
        if(pOnboardData->u8CountDeactiveBLE == 0){
            if(BlueSetupWiFi_GetLink() != BlueSetupWiFi_Link_CONNECTED){
                // 如果蓝牙已经连接上了客户端,保持不断开
                StopBleAdv();
                pOnboardData->bActiveBLE = false;
                esp_timer_stop(pOnboardData->hTimer);
            }
        }
        break;
    default:
        break;
    }
}

static void on_timer(void *pArgs)
{
    onboard_data_t *pOnboardData = (onboard_data_t*)pArgs;
    esp_event_isr_post_to(pOnboardData->hEventLoop, ONBOARD_EVENT, ONBOARD_EVENT_TICK, NULL, 0, 0);
}

void ir_btn_active_ble(void)
{
    esp_event_post_to(s_data.hEventLoop, ONBOARD_EVENT, ONBOARD_EVENT_BTN_ACTIVE_BLE, NULL, 0, 0);
}

static void read_wifi_cfg(char *bufWiFi, size_t len)
{
    // read wifi configuration
    const char *szWiFi = BlueSetupWiFi_NVS_Read();
    size_t size = strlen(szWiFi);
    size = size < len-1 ? size : len-1;
    strcpy(bufWiFi, szWiFi);
}

void init_onboard_btn(esp_event_loop_handle_t hEventLoop, bool bActiveBLE, char *bufWiFi, size_t len)
{
    assert(s_data.hOnEvent == NULL);

    esp_timer_create_args_t timer_args = {
        .callback = on_timer,
        .arg = &s_data,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "onboard_button",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_data.hTimer));

    s_data.hEventLoop = hEventLoop;
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        hEventLoop, ONBOARD_EVENT, ESP_EVENT_ANY_ID, on_event, &s_data, &s_data.hOnEvent));

    gpio_config_t gpio_boot_btn = {
        .pin_bit_mask = GPIO_SEL_0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };

    ESP_ERROR_CHECK(gpio_config(&gpio_boot_btn));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_handler, &s_data);
    
    ESP_ERROR_CHECK(BlueSetupWiFi_Init(false));
    read_wifi_cfg(bufWiFi, len);

    if(bActiveBLE){
        BlueSetupWiFi_Start(c_szName);
        s_data.u8CountDeactiveBLE = MAX_ACTIVE_SECONDS;
        s_data.bActiveBLE = true;
        ESP_ERROR_CHECK(esp_timer_start_periodic(s_data.hTimer, 1000000));
    }
    else{
        ESP_ERROR_CHECK(BlueSetupWiFi_Deinit(false));
    }
}

void StartBleAdv()
{
    // WiFi provisioning via BT_BLE GATT
    ESP_ERROR_CHECK(BlueSetupWiFi_Init(false));
    BlueSetupWiFi_Start(c_szName);
}

void StopBleAdv(void)
{
    ESP_ERROR_CHECK(BlueSetupWiFi_Stop());
    ESP_ERROR_CHECK(BlueSetupWiFi_Deinit(false));
}