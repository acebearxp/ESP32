#include <stdio.h>
#include <esp_log.h>
#include <esp_err.h>
#include "BlueSetupWiFi.h"
#include "WiFiSTA.h"
#include "blink.h"

#define BLINK_TIM_GROUP 0
#define BLINK_TIM_INDEX 1

typedef struct _blink_data_struct{
    bool bAuto;
    BlinkStatus status;
    BlueSetupWiFiLink link;
    uint32_t u32IPv4;
} blink_data_t;

static blink_data_t s_data;

inline static void _set(BlinkStatus status)
{
    if(s_data.status != status){
        s_data.status = status;
        BlinkStatus_Set(status);
    }
}

static void _set_auto()
{
    BlinkStatus status;
    bool bHasIP = (s_data.u32IPv4 != 0);

    switch(s_data.link){
        case BlueSetupWiFi_Link_STOP:
            status = bHasIP ? BLINK_STATUS_WIFI_STANDBY : BLINK_STATUS_POWER_ON;
            break;
        case BlueSetupWiFi_Link_ADVERTISING:
            status = bHasIP ? BLINK_STATUS_BTWF_STANDBY : BLINK_STATUS_BT_STANDBY;
            break;
        case BlueSetupWiFi_Link_CONNECTED:
            status = BLINK_STATUS_BT_CONN;
            break;
        default:
            assert(false);
    }
    _set(status);
}

static void _on_bt_changed(BlueSetupWiFiLink link)
{
    s_data.link = link;
    if(s_data.bAuto) _set_auto();
}

static void _on_ip_changed(esp_netif_ip_info_t *pIP)
{
    s_data.u32IPv4 = (pIP==NULL ? 0 : pIP->ip.addr);
    if(s_data.bAuto) _set_auto();

    BlueSetupWiFi_SetIPv4(s_data.u32IPv4);
}

void blink_init(esp_event_loop_handle_t hEventLoop)
{
    BlinkStatus_Init(hEventLoop);
    BlueSetupWiFi_Set_OnStatusChanged(_on_bt_changed);
    WiFi_STA_Set_OnIPChanged(_on_ip_changed);
}

void blink_set(BlinkStatus status)
{
    s_data.bAuto = false;
    _set(status);
}

void blink_auto(void)
{
    s_data.bAuto = true;
    _set_auto();
}