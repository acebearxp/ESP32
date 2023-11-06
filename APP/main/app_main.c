#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_sntp.h>
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include "WiFiSTA.h"
#include "onboard.h"
#include "extra.h"
#include "blink.h"
#include "task_tm.h"
#include "task_uart.h"
#include "task_i2c.h"
#include "IrRC.h"
#include "motor.h"

static const char c_szTAG[] = "APP";

void ntp_start(void)
{
    // NTP
    setenv("TZ", "GMT-8", 1);
    tzset();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "2.cn.pool.ntp.org");
    esp_sntp_setservername(1, "pool.ntp.org");
    esp_sntp_init();

    char szTm[64];
    uint8_t u8x = 0;
    while(u8x < 5){
        if(get_tm(szTm, sizeof(szTm))){
            ESP_LOGI(c_szTAG, "NTP Time: %s", szTm);
            break;
        }
        u8x++;
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

static MotorX4_handler_t s_hMotorX4 = NULL;

void motorx4_start(void)
{
    MotorX4_Config_t cfg = {
        .motorLeftFront = {
            .pinVCC = GPIO_NUM_15,
            .pinSLEEP = GPIO_NUM_16,
            .pinIN1 = GPIO_NUM_17,
            .pinIN2 = GPIO_NUM_18
        },
        .motorLeftRear = {
            .pinVCC = GPIO_NUM_9,
            .pinSLEEP = GPIO_NUM_10,
            .pinIN1 = GPIO_NUM_11,
            .pinIN2 = GPIO_NUM_12
        },
        .motorRightFront = {
            .pinIN2 = GPIO_NUM_42,
            .pinIN1 = GPIO_NUM_41,
            .pinSLEEP = GPIO_NUM_40,
            .pinVCC = GPIO_NUM_39
        },
        .motorRightRear = {
            .pinIN2 = GPIO_NUM_38,
            .pinIN1 = GPIO_NUM_37,
            .pinSLEEP = GPIO_NUM_36,
            .pinVCC = GPIO_NUM_35
        }
    };
    
    s_hMotorX4 = MotorX4_driver_install(&cfg);
    // 四个电机内置有减速齿轮,滑行很吃力,因此只能以4WD方式驱动
    MotorX4_SwtichDrive(s_hMotorX4, DriveMode_4WD);
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
            LogTasksInfo(c_szTAG, true);
            break;
        case 8:
        {
            // 方向上键
            break;
        }
        case 13:
        {
            // Right
            break;
        }
        case 16:
        {
            // Left
            break;
        }
        case 20:
            break;
        default:
            break;
        }
    }
    else if(u16Address == 1){
        static int8_t n8speed = 0;
        // SONY TV Remote Control
        switch (u8Code)
        {
        case 116: // 方向上键
            n8speed = 80;
            MotorX4_Drive(s_hMotorX4, n8speed);
            break;
        case 117: // 方向下键
            n8speed = -80;
            MotorX4_Drive(s_hMotorX4, n8speed);
            break;
        case 101: // 中间键
            MotorX4_Break(s_hMotorX4);
            break;
        case 51: // 方向右键
            break;
        case 52: // 方向左键
            break;
        case 18: // 音量+
            n8speed += 3;
            if(n8speed > 100) n8speed = 100;
            // 占空比太低,轮子不转,因此倒退时最低设定为80
            if(n8speed < 0 && n8speed > -80) n8speed = -80;
            MotorX4_Drive(s_hMotorX4, n8speed);
            break;
        case 19: // 音量-
            n8speed -= 3;
            if(n8speed < -100) n8speed = -100;
            // 占空比太低,轮子不转,因此前进时最低设定为80
            if(n8speed > 0 && n8speed < 80) n8speed = 80;
            MotorX4_Drive(s_hMotorX4, n8speed);
            break;
        case 37: // 输入选择
            break;
        default:
            break;
        }
    }
}

void task_start(void *pArgs)
{
    esp_event_loop_handle_t hEventLoop = (esp_event_loop_handle_t)pArgs;

    // wait 3 seconds
    vTaskDelay(pdMS_TO_TICKS(3000));
    blink_auto();

    // UART & I2C
    // start_uart(hEventLoop, on_ir_data);
    // start_i2c(hEventLoop);

    // IrRC
    gpio_config_t ir_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_4) | BIT64(GPIO_NUM_5),
        .mode = GPIO_MODE_OUTPUT
    };
    ESP_ERROR_CHECK(gpio_config(&ir_config));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_5, 0)); // G
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_4, 1)); // V
    IrRC_Init(hEventLoop, GPIO_NUM_6); // GPIO_6 DATA
    IrRC_Set_OnData(GPIO_NUM_6, on_ir_data, 0);

    nvs_start();
    char szWiFi[512] = {0};
    init_onboard_btn(hEventLoop, false, szWiFi, 512);

    motorx4_start();

    if(strlen(szWiFi) > 0){
        // WiFi Startup
        ESP_ERROR_CHECK(WiFi_STA_Init());
        ESP_ERROR_CHECK(WiFi_STA_SetConfig(szWiFi));
        ESP_ERROR_CHECK(WiFi_STA_Start());
        ESP_ERROR_CHECK(WiFi_STA_Connect(true));

        esp_netif_ip_info_t info;
        WiFi_STA_GetIP(&info);
        ESP_LOGI(c_szTAG, "IPv4 %d.%d.%d.%d", IP2STR(&info.ip));

        ntp_start();
        // start_udp_test(hEventLoop);
    }

    LogTasksInfo(c_szTAG, true);

    vTaskDelete(NULL);
}

esp_event_loop_handle_t create_event_loop()
{
    esp_event_loop_args_t args = {
        .queue_size = 32,
        .task_name = NULL,
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

    // starting
    BaseType_t xRet = xTaskCreate(task_start, "task_start", 4096, hEventLoop, 1, NULL);
    assert(xRet == pdPASS);

    ESP_LOGI(c_szTAG, "app_main loop starting...");

    char szTm[64];
    uint8_t u8Count = 0;
    while(1){
        if(u8Count % 30 == 15){
        }
        else if(u8Count % 30 == 29){
        }

        esp_event_loop_run(hEventLoop, pdMS_TO_TICKS(1000));
        u8Count++;

        if(u8Count == 0){
            LogTasksInfo(c_szTAG, false);
            get_tm(szTm, sizeof(szTm));
            ESP_LOGI(c_szTAG, "%s app_main loop is still running...", szTm);
        }
    }

    ESP_LOGI(c_szTAG, "app_main finished.");
}
