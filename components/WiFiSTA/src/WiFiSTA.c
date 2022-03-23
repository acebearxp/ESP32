#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include "WiFiSTA.h"

#define MAX_RETRY_COUNT_PER_SSID   3

#define WIFI_STA_CHANGED_BIT    BIT0
#define WIFI_CONN_CHANGED_BIT   BIT1
#define IP_CONN_CHANGED_BIT     BIT2
#define CONNECT_RESULT_BIT      BIT3
#define CTRL_QUIT_BIT           BIT4
#define TASK_QUIT_BIT           BIT5

static const char c_szTAG[] = "WiFiSTA";

typedef struct _ssid_pwd_struct{
    char *szSSID;
    char *szPWD;
} ssid_pwd_t;

typedef struct _wifi_sta_data_struct{
    EventGroupHandle_t evgWiFi;
    esp_netif_t *pNetIf;
    esp_event_handler_instance_t hander_wifi;
    esp_event_handler_instance_t hander_ip;
    bool bAutoReconnect;
    TaskHandle_t task_connect;
    uint8_t u8Count; // the count of entries in pListSP
    uint8_t u8Idx;
    ssid_pwd_t *pListSP;
    bool bSTAReady;
    bool bSTAConnected;
    bool bIPConnected;
    WiFi_STA_OnIPChanged onIPChanged;
} wifi_sta_data_t;

static wifi_sta_data_t s_wifi_sta_data;

static void on_networking_event(void *pArg, esp_event_base_t ev, int32_t evid, void *pData)
{
    wifi_sta_data_t *pWiFiData = (wifi_sta_data_t*)pArg;

    if(ev == WIFI_EVENT){
        switch(evid){
            case WIFI_EVENT_STA_START:
                pWiFiData->bSTAReady = true;
                xEventGroupSetBits(pWiFiData->evgWiFi, WIFI_STA_CHANGED_BIT);
                break;
            case WIFI_EVENT_STA_STOP:
                pWiFiData->bSTAReady = false;
                xEventGroupSetBits(pWiFiData->evgWiFi, WIFI_STA_CHANGED_BIT);
                break;
            case WIFI_EVENT_STA_CONNECTED:
                pWiFiData->bSTAConnected = true;
                ESP_LOGI(c_szTAG, "WiFi connected");
                xEventGroupSetBits(pWiFiData->evgWiFi, WIFI_CONN_CHANGED_BIT);
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
            {
                pWiFiData->bSTAConnected = false;
                wifi_event_sta_disconnected_t *pEvData = (wifi_event_sta_disconnected_t*)pData;
                ESP_LOGI(c_szTAG, "WiFi disconnected with reason %u", pEvData->reason);
                xEventGroupSetBits(pWiFiData->evgWiFi, WIFI_CONN_CHANGED_BIT);
                break;
            }
            case WIFI_EVENT_STA_BEACON_TIMEOUT:
                // 信号弱???
                ESP_LOGW(c_szTAG, "WiFi beacon timeout");
                break;
            default:
                ESP_LOGW(c_szTAG, "unprocessed wifi_event: %d", evid);
        }
    }
    else if(ev == IP_EVENT){
        switch(evid){
            case IP_EVENT_STA_GOT_IP:
            {
                pWiFiData->bIPConnected = true;
                xEventGroupSetBits(pWiFiData->evgWiFi, IP_CONN_CHANGED_BIT);
                if(pWiFiData->onIPChanged){
                    ip_event_got_ip_t *pEvData = (ip_event_got_ip_t*)pData;
                    // ESP_LOGI(c_szTAG, "IP %d.%d.%d.%d", IP2STR(&pEvData->ip_info.ip));
                    pWiFiData->onIPChanged(&pEvData->ip_info);
                }
                break;
            }
            case IP_EVENT_STA_LOST_IP:
                pWiFiData->bIPConnected = false;
                xEventGroupSetBits(pWiFiData->evgWiFi, IP_CONN_CHANGED_BIT);
                if(pWiFiData->onIPChanged){
                    pWiFiData->onIPChanged(NULL);
                }
                break;
            default:
            ESP_LOGW(c_szTAG, "unprocessed ip_event: %d", evid);
        }
    }
    else{
        ESP_LOGW(c_szTAG, "unexpected event %s %d", ev, evid);
    }
}

static esp_err_t connect(uint8_t u8Idx);

static void task_connect(void *pArgs)
{
    wifi_sta_data_t *pWiFiData = (wifi_sta_data_t*)pArgs;

    const uint8_t c_u8FastRetryMax = pWiFiData->u8Count * MAX_RETRY_COUNT_PER_SSID;
    uint8_t u8RetryCount = 0, u8Idx = pWiFiData->u8Idx;
    TickType_t tickWait = pdMS_TO_TICKS(5000);

    while(true){
        EventBits_t bits = xEventGroupWaitBits(pWiFiData->evgWiFi,
            WIFI_STA_CHANGED_BIT|WIFI_CONN_CHANGED_BIT|IP_CONN_CHANGED_BIT|CTRL_QUIT_BIT,
            pdTRUE, pdFALSE, tickWait);
        
        ESP_LOGD(c_szTAG, "====>: 0x%02X auto:%u sta:%u-%u ip:%u",
                bits,
                pWiFiData->bAutoReconnect,
                pWiFiData->bSTAReady,
                pWiFiData->bSTAConnected,
                pWiFiData->bIPConnected);
        
        if(bits & CTRL_QUIT_BIT) break;
        
        if(pWiFiData->bIPConnected && pWiFiData->bSTAConnected){
            // connect succeeded
            pWiFiData->u8Idx = u8Idx;
            // reset retry count
            u8RetryCount = 0;
            tickWait = portMAX_DELAY;
            xEventGroupSetBits(pWiFiData->evgWiFi, CONNECT_RESULT_BIT);
        }
        else if(pWiFiData->bSTAReady && !pWiFiData->bSTAConnected){
            if(u8RetryCount < c_u8FastRetryMax){
                if(u8RetryCount > 0)
                    u8Idx = (u8Idx+1) % pWiFiData->u8Count;
                ESP_ERROR_CHECK(connect(u8Idx));
                u8RetryCount++;
                tickWait = pdMS_TO_TICKS(5000);
            }
            else{
                if(u8RetryCount ==  c_u8FastRetryMax) // connect failed
                    xEventGroupSetBits(pWiFiData->evgWiFi, CONNECT_RESULT_BIT);
                
                // switch to slow retry
                if((u8RetryCount-c_u8FastRetryMax) % 6 == 5){
                    u8Idx = (u8Idx+1) % pWiFiData->u8Count;
                    ESP_ERROR_CHECK(connect(u8Idx));
                }
                u8RetryCount++;
                if(u8RetryCount == 0xff) u8RetryCount = c_u8FastRetryMax;
            }
        }
        else if(pWiFiData->bSTAConnected){
            ESP_LOGI(c_szTAG, "waiting to get ip address...");
        }
        else{
            ESP_LOGW(c_szTAG, "WARNNING: 0x%02X auto:%u sta:%u-%u ip:%u",
                bits,
                pWiFiData->bAutoReconnect,
                pWiFiData->bSTAReady,
                pWiFiData->bSTAConnected,
                pWiFiData->bIPConnected);
            tickWait = pdMS_TO_TICKS(60000);
        }
    }
    
    xEventGroupSetBits(pWiFiData->evgWiFi, TASK_QUIT_BIT);
    vTaskDelete(NULL);
}

void set_wifi_ssid_cfg(wifi_config_t *pCfg, uint8_t u8Idx)
{
    memset(pCfg->sta.ssid, 0, sizeof(pCfg->sta.ssid));
    memset(pCfg->sta.password, 0, sizeof(pCfg->sta.password));
    memcpy(pCfg->sta.ssid, s_wifi_sta_data.pListSP[u8Idx].szSSID, strlen(s_wifi_sta_data.pListSP[u8Idx].szSSID));
    memcpy(pCfg->sta.password, s_wifi_sta_data.pListSP[u8Idx].szPWD, strlen(s_wifi_sta_data.pListSP[u8Idx].szPWD));
}

esp_err_t connect(uint8_t u8Idx)
{
    // config ssid
    wifi_config_t wifi_cfg;
    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg));
    set_wifi_ssid_cfg(&wifi_cfg, u8Idx);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    ESP_LOGI(c_szTAG, "connecting to [%s]", s_wifi_sta_data.pListSP[u8Idx].szSSID);
    return esp_wifi_connect();
}

esp_err_t WiFi_STA_Init()
{
    s_wifi_sta_data.evgWiFi = xEventGroupCreate();
    // prepare networking event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // wifi station mode supporting
    s_wifi_sta_data.pNetIf = esp_netif_create_default_wifi_sta();
    // wifi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // esp-lwIP
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        on_networking_event, &s_wifi_sta_data, &s_wifi_sta_data.hander_wifi));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
        on_networking_event, &s_wifi_sta_data, &s_wifi_sta_data.hander_ip));

    return ESP_OK;
}

esp_err_t WiFi_STA_Deinit()
{
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, s_wifi_sta_data.hander_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_sta_data.hander_wifi));

    WiFi_STA_SetConfig(NULL);

    esp_err_t espRet = esp_netif_deinit();
    if(espRet != ESP_ERR_NOT_SUPPORTED){
        // lwIP does not support deinit for now
        ESP_ERROR_CHECK(espRet);
    }

    ESP_ERROR_CHECK(esp_wifi_deinit());
    esp_netif_destroy_default_wifi(s_wifi_sta_data.pNetIf);
    
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    vEventGroupDelete(s_wifi_sta_data.evgWiFi);

    return ESP_OK;
}

uint8_t count_wifi_ssid(const char *szWiFi)
{
    // scan \n for count
    uint8_t u8Count = 0;
    const char *p = szWiFi;
    while(*p){
        if(*p == '\n') u8Count++;
        p++;
    }
    // need +1 because  the ending '\n' is optional
    // 3 or 4 '\n' both stand for 2 ssid_pwd pair
    u8Count = (u8Count+1) >> 1;
    return u8Count;
}

ssid_pwd_t* create_wifi_ssid(const char *szWiFi, uint8_t *pu8Count)
{
    // structure : ps0|pw0|ps1|pw1|...|ssid0\0pwd0\0ssid1\0pwd1\0ssidn\0pwdn\0...
    *pu8Count = count_wifi_ssid(szWiFi);
    if(*pu8Count == 0) return NULL;

    uint16_t u16SizeList = (*pu8Count) * sizeof(ssid_pwd_t);
    uint16_t u16SizeTotal = u16SizeList + strlen(szWiFi) + 1;
    char *pData = malloc(u16SizeTotal);

    // copy content
    char *pX = pData + u16SizeList;
    strcpy(pX, szWiFi); // actual content

    char **pD = (char**)pData;
    uint8_t u8N = 0, u8NTotal = (*pu8Count) << 1;
    *pD = pX;
    while(pX < (char*)pData + u16SizeTotal){
        if(*pX == '\n'){
            *pX = 0;
            u8N++;
            if(u8N < u8NTotal){
                pD++;
                *pD = pX+1;
            }
        }
        pX++;
    }

    return (ssid_pwd_t*)pData;
}

void destroy_wifi_ssid(ssid_pwd_t *pData)
{
    free(pData);
}

esp_err_t WiFi_STA_SetConfig(const char *pszSPL)
{
    if(pszSPL != NULL){
        s_wifi_sta_data.pListSP = create_wifi_ssid(pszSPL, &s_wifi_sta_data.u8Count);
        if(s_wifi_sta_data.u8Count == 0) return ESP_ERR_INVALID_ARG;

        // checking length
        wifi_config_t wifi_cfg;
        for(uint8_t i=0;i<s_wifi_sta_data.u8Count;i++){
            ESP_LOGI(c_szTAG, "Loaded ssid %u -> %s", i, s_wifi_sta_data.pListSP[i].szSSID);
            assert(strlen(s_wifi_sta_data.pListSP[i].szSSID) <= sizeof(wifi_cfg.sta.ssid));
            assert(strlen(s_wifi_sta_data.pListSP[i].szPWD) <= sizeof(wifi_cfg.sta.password));
        }
    }
    else{
        destroy_wifi_ssid(s_wifi_sta_data.pListSP);
        s_wifi_sta_data.pListSP = NULL;
    }

    s_wifi_sta_data.u8Idx = 0;
    return ESP_OK;
}

esp_err_t WiFi_STA_Start()
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    wifi_config_t wifi_cfg = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            }
        }
    };

    // TODO: start without setting ssid perhaps be okay
    set_wifi_ssid_cfg(&wifi_cfg, s_wifi_sta_data.u8Idx);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    xEventGroupWaitBits(s_wifi_sta_data.evgWiFi,
            WIFI_STA_CHANGED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));

    return s_wifi_sta_data.bSTAReady?ESP_OK:ESP_FAIL;
}

esp_err_t WiFi_STA_Stop()
{
    ESP_ERROR_CHECK(esp_wifi_stop());

    xEventGroupWaitBits(s_wifi_sta_data.evgWiFi,
            WIFI_STA_CHANGED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
    
    return (s_wifi_sta_data.bSTAConnected == false ? ESP_OK : ESP_FAIL);
}

static void stop_task_wifi_connect(void)
{
    xEventGroupSetBits(s_wifi_sta_data.evgWiFi, CTRL_QUIT_BIT);
    EventBits_t bits = xEventGroupWaitBits(s_wifi_sta_data.evgWiFi,
            TASK_QUIT_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
    if(!(bits & TASK_QUIT_BIT)){
        ESP_LOGW(c_szTAG, "task wifi_connect doesn't quit in time");
        vTaskDelete(s_wifi_sta_data.task_connect);
    }
    s_wifi_sta_data.task_connect = NULL;
}

esp_err_t WiFi_STA_Connect(bool bAutoReconnect)
{
    if(s_wifi_sta_data.task_connect) return ESP_ERR_INVALID_STATE;

    s_wifi_sta_data.bAutoReconnect = bAutoReconnect;
    BaseType_t xRet = xTaskCreate(task_connect, "wifi_connect", 4096, &s_wifi_sta_data, 5, &s_wifi_sta_data.task_connect);
    assert(xRet == pdPASS);

    xEventGroupWaitBits(s_wifi_sta_data.evgWiFi, CONNECT_RESULT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    if(!(s_wifi_sta_data.bIPConnected && bAutoReconnect)){
        stop_task_wifi_connect();
    }
    
    return s_wifi_sta_data.bIPConnected ? ESP_OK : ESP_ERR_WIFI_CONN;
}

esp_err_t WiFi_STA_Disconnect()
{
    // stop task wifi_connect
    if(s_wifi_sta_data.bAutoReconnect){
        stop_task_wifi_connect();
    }

    // disconnect
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    xEventGroupWaitBits(s_wifi_sta_data.evgWiFi,
            WIFI_CONN_CHANGED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
    
    return (s_wifi_sta_data.bSTAConnected ? ESP_FAIL: ESP_OK);
}

esp_err_t WiFi_STA_GetIP(esp_netif_ip_info_t *pIP)
{
    return esp_netif_get_ip_info(s_wifi_sta_data.pNetIf, pIP);
}

inline bool WiFi_STA_IsIPConnected(void)
{
    return s_wifi_sta_data.bIPConnected && s_wifi_sta_data.bSTAConnected;
}

void WiFi_STA_Set_OnIPChanged(WiFi_STA_OnIPChanged onIPChanged)
{
    s_wifi_sta_data.onIPChanged = onIPChanged;
}