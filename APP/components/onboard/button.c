#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <driver/gpio.h>
#include "BlueSetupWiFi.h"
#include "button.h"

#define MAX_ACTIVE_SECONDS 30

static QueueHandle_t s_queue;
static bool s_bActiveBLE = false;
static uint8_t s_u8Count = 0;

static void IRAM_ATTR gpio_isr_handler(void *pArgs)
{
    uint8_t u8Data = 0;
    xQueueSendToBackFromISR(s_queue, &u8Data, 0);
}

static void task_boot_btn(void *pArgs)
{
    uint8_t u8Data = 1;
    TickType_t tickWait = portMAX_DELAY;
    while(true){
        if(xQueueReceive(s_queue, &u8Data, tickWait) == pdTRUE){
            s_u8Count = MAX_ACTIVE_SECONDS;
            if(!s_bActiveBLE){
                StartBleAdv(u8Data);
                s_bActiveBLE = true;
                tickWait = pdMS_TO_TICKS(1000);
            }
        }
        else{
            if(s_u8Count > 0) s_u8Count--;

            if(s_u8Count == 0){
                if(BlueSetupWiFi_GetLink() != BlueSetupWiFi_Link_CONNECTED){
                    // 如果蓝牙已经连接上了客户端,保持不断开
                    StopBleAdv();
                    s_bActiveBLE = false;
                    tickWait = portMAX_DELAY;
                }
            }
        }
    }
}

void ir_btn_active_ble(void)
{
    uint8_t u8Data = 1;
    xQueueSendToBack(s_queue, &u8Data, 0);
}

void boot_btn_active_ble(void)
{
    assert(s_queue == NULL);
    s_queue = xQueueCreate(8, sizeof(uint8_t));
    xTaskCreate(task_boot_btn, "boot_btn", 4096, 0, 3, 0);

    gpio_config_t gpio_boot_btn = {
        .pin_bit_mask = GPIO_SEL_0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };

    ESP_ERROR_CHECK(gpio_config(&gpio_boot_btn));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_handler, 0);

    uint8_t u8Data = 0;
    xQueueSendToBack(s_queue, &u8Data, 0);
}

void StartBleAdv(uint8_t u8Flag)
{
    // WiFi provisioning via BT_BLE GATT
    ESP_LOGI("task_boot_btn", "StartBleAdv()");
    ESP_ERROR_CHECK(BlueSetupWiFi_Init(false));
    BlueSetupWiFi_Start("AceBear-ESP32S3-DevKitC-1");
}

void StopBleAdv(void)
{
    ESP_LOGI("task_boot_btn", "StopBleAdv()");
    ESP_ERROR_CHECK(BlueSetupWiFi_Stop());
    ESP_ERROR_CHECK(BlueSetupWiFi_Deinit(false));
}