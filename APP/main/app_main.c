#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_sntp.h>
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include "BlueSetupWiFi.h"
#include "WiFiSTA.h"
#include "onboard.h"
#include "extra.h"
#include "blink.h"
#include "task_tm.h"
#include "IrRC.h"

static const char c_szTAG[] = "APP";

void ntp_start(void)
{
    // NTP
    setenv("TZ", "GMT-8", 1);
    tzset();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "2.cn.pool.ntp.org");
    sntp_setservername(1, "pool.ntp.org");
    sntp_init();

    char szTm[64];
    uint8_t u8x = 0;
    while(u8x < 5){
        if(get_tm(szTm, sizeof(szTm))){
            ESP_LOGI(c_szTAG, "NTP Time: %s", szTm);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void nvs_start(void)
{
    esp_err_t espRet = nvs_flash_init();
    if(espRet == ESP_ERR_NVS_NO_FREE_PAGES || espRet == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_LOGI(c_szTAG, "nvs initialized");
}

void on_ir_data(gpio_num_t gpio, uint16_t u16Address, uint8_t u8Code, void *pArgs)
{
    ESP_LOGI(c_szTAG, "on_ir_data: %u => %u", u16Address, u8Code);
    if(u16Address == 3704){
        switch(u8Code){
        case 28:
            // 手边的一块红外遥控器上的标记蓝牙的一个按钮(同时是漫步者音箱的遥控器)
            ir_btn_active_ble();
            break;
        case 25:
            // opt按钮
            LogTasksInfo(c_szTAG);
            break;
        default:
            break;
        }
    }
}

esp_event_loop_handle_t create_event_loop()
{
    esp_event_loop_args_t args = {
        .queue_size = 32,
        .task_name = "app_event_loop",
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY
    };

    esp_event_loop_handle_t hEventLoop;
    ESP_ERROR_CHECK(esp_event_loop_create(&args, &hEventLoop));
    return hEventLoop;
}

void app_main(void)
{
    ESP_LOGI(c_szTAG, "app_main start...");
    esp_event_loop_handle_t hEventLoop = create_event_loop();

    blink_init(hEventLoop);
    blink_set(BLINK_STATUS_POWER_ON);
    // wait 3 seconds
    vTaskDelay(pdMS_TO_TICKS(3000));
    blink_auto();

    // IrRC
    IrRC_Init(hEventLoop, 4, 7, 5); // RMT_RX_7---GPIO_4
    IrRC_Set_OnData(4, on_ir_data, 0);

    nvs_start();
    boot_btn_active_ble();
    const char *szWiFi = BlueSetupWiFi_NVS_Read();

    if(szWiFi != NULL){
        // WiFi Startup
        ESP_ERROR_CHECK(WiFi_STA_Init());
        ESP_ERROR_CHECK(WiFi_STA_SetConfig(szWiFi));
        ESP_ERROR_CHECK(WiFi_STA_Start());
        ESP_ERROR_CHECK(WiFi_STA_Connect(true));

        esp_netif_ip_info_t info;
        WiFi_STA_GetIP(&info);
        ESP_LOGI(c_szTAG, "IPv4 %d.%d.%d.%d", IP2STR(&info.ip));

        ntp_start();
        assert(xTaskCreate(task_udp_test, "udp_test", 4096, 0, 3, 0) == pdPASS);
    }

    LogTasksInfo(c_szTAG);
    ESP_LOGI(c_szTAG, "app_main loop starting...");

    char szTm[64];
    uint8_t u8Count = 0;
    while(1){
        // esp_event_loop_run(hEventLoop, pdMS_TO_TICKS(1000));
        vTaskDelay(pdMS_TO_TICKS(1000));
        u8Count++;

        if(u8Count == 0){
            LogTasksInfo(c_szTAG);
            get_tm(szTm, sizeof(szTm));
            ESP_LOGI(c_szTAG, "%s app_main loop is still running...", szTm);
        }
    }

    ESP_LOGI(c_szTAG, "app_main finished.");
}
