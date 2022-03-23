#pragma once

#include <stdio.h>
#include <esp_system.h>
#include <esp_err.h>

typedef enum BlueSetupWiFi_Link_Enum{
    BlueSetupWiFi_Link_STOP,
    BlueSetupWiFi_Link_ADVERTISING,
    BlueSetupWiFi_Link_CONNECTED
} BlueSetupWiFiLink;

esp_err_t BlueSetupWiFi_Init(bool bInitNvs);
esp_err_t BlueSetupWiFi_Deinit(bool bDeinitNvs);

void BlueSetupWiFi_Start(const char* szDevName);
esp_err_t BlueSetupWiFi_Stop(void);

const char* BlueSetupWiFi_NVS_Read();
const char* BlueSetupWiFi_NVS_Save(char *szConfig);

const char* BlueSetupWiFi_GetSPLConfig(void);

BlueSetupWiFiLink BlueSetupWiFi_GetLink(void);
void BlueSetupWiFi_SetLink(BlueSetupWiFiLink link);

typedef void (*BlueSetupWiFi_OnStatusChanged)(BlueSetupWiFiLink link);
void BlueSetupWiFi_Set_OnStatusChanged(BlueSetupWiFi_OnStatusChanged onStatusChanged);
BlueSetupWiFi_OnStatusChanged BlueSetupWiFi_Get_OnStatusChanged(void);

void BlueSetupWiFi_SetIPv4(uint32_t u32IPv4);
uint32_t BlueSetupWiFi_GetIPv4(void);