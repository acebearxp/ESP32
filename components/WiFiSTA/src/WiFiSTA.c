#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include "WiFiSTA.h"

#define MAX_RETRY_COUNT_PER_SSID    3
#define MAX_WAITING_FOR_IP_SECONDS  8

#define WIFI_STA_CHANGED_BIT    BIT0
#define CONNECT_RESULT_BIT      BIT1

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
    esp_timer_handle_t hTimerWaitForIP;
    bool bAutoReconnect;
    uint8_t u8RetryCount;
    uint8_t u8Count; // the count of entries in pListSP
    uint8_t u8Idx;
    ssid_pwd_t *pListSP;
    bool bSTAReady;
    bool bSTAConnected;
    bool bIPConnected;
    WiFi_STA_OnIPChanged onIPChanged;
} wifi_sta_data_t;

static wifi_sta_data_t s_wifi_sta_data;

static esp_err_t connect(uint8_t u8Idx);
static void check_for_reconnect(wifi_sta_data_t *pWiFiData, uint8_t u8Reason);

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
                // 通常过几秒会有IP_EVENT_STA_GOT_IP
                // 但如果长时间拿不到IP,需要断开重试
                esp_timer_start_once(pWiFiData->hTimerWaitForIP, 1000000*MAX_WAITING_FOR_IP_SECONDS); // 8 seconds
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
            {
                pWiFiData->bSTAConnected = false;
                wifi_event_sta_disconnected_t *pEvData = (wifi_event_sta_disconnected_t*)pData;
                ESP_LOGI(c_szTAG, "WiFi disconnected with reason %u", pEvData->reason);
                check_for_reconnect(pWiFiData, pEvData->reason);
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
                pWiFiData->u8RetryCount = 0;
                esp_timer_stop(pWiFiData->hTimerWaitForIP);
                xEventGroupSetBits(pWiFiData->evgWiFi, CONNECT_RESULT_BIT);
                if(pWiFiData->onIPChanged){
                    ip_event_got_ip_t *pEvData = (ip_event_got_ip_t*)pData;
                    // ESP_LOGI(c_szTAG, "IP %d.%d.%d.%d", IP2STR(&pEvData->ip_info.ip));
                    pWiFiData->onIPChanged(&pEvData->ip_info);
                }
                break;
            }
            case IP_EVENT_STA_LOST_IP:
                pWiFiData->bIPConnected = false;
                // 等待一会儿,看看是否可以重获IP
                esp_timer_start_once(pWiFiData->hTimerWaitForIP, 1000000*MAX_WAITING_FOR_IP_SECONDS); // 8 seconds
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


static void on_timer_waiting_for_ip(void *pArgs)
{
    wifi_sta_data_t *pWiFiData = (wifi_sta_data_t*)pArgs;
    if(!pWiFiData->bIPConnected){
        check_for_reconnect(pWiFiData, WIFI_REASON_BEACON_TIMEOUT);
    }
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
    s_wifi_sta_data.u8RetryCount++;
    return esp_wifi_connect();
}

static void check_for_reconnect(wifi_sta_data_t *pWiFiData, uint8_t u8Reason)
{
    if(u8Reason == WIFI_REASON_ASSOC_LEAVE){
        // 主动发起的断开
        xEventGroupSetBits(pWiFiData->evgWiFi, CONNECT_RESULT_BIT);
    }
    else if(pWiFiData->u8RetryCount >= pWiFiData->u8Count * MAX_RETRY_COUNT_PER_SSID){
        if(pWiFiData->bAutoReconnect){
            // 重试
            pWiFiData->u8Idx = (pWiFiData->u8Idx+1) % pWiFiData->u8Count;
            connect(pWiFiData->u8Idx);
        }
        else{
            // 连接失败
            xEventGroupSetBits(pWiFiData->evgWiFi, CONNECT_RESULT_BIT);
        }
    }
    else{
        // 重试
        pWiFiData->u8Idx = (pWiFiData->u8Idx+1) % pWiFiData->u8Count;
        connect(pWiFiData->u8Idx);
    }
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
    
    // timer waiting for ip
    esp_timer_create_args_t timer_args = {
        .callback = on_timer_waiting_for_ip,
        .arg = &s_wifi_sta_data,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "on_timer_waiting_for_ip",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_wifi_sta_data.hTimerWaitForIP));

    return ESP_OK;
}

esp_err_t WiFi_STA_Deinit()
{
    ESP_ERROR_CHECK(esp_timer_delete(s_wifi_sta_data.hTimerWaitForIP));

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

esp_err_t WiFi_STA_Connect(bool bAutoReconnect)
{
    ESP_ERROR_CHECK(connect(s_wifi_sta_data.u8Idx));
    xEventGroupWaitBits(s_wifi_sta_data.evgWiFi, CONNECT_RESULT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    if(s_wifi_sta_data.bIPConnected){
        s_wifi_sta_data.bAutoReconnect = bAutoReconnect;
        return ESP_OK;
    }
    else{
        return ESP_ERR_WIFI_CONN;
    }
}

esp_err_t WiFi_STA_Disconnect()
{
    // disconnect
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    xEventGroupWaitBits(s_wifi_sta_data.evgWiFi,
            CONNECT_RESULT_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
    
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