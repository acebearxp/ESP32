/*
    The ESP32-S3-DevKitC-1 board has a built-in RGB LED connected to GPIO48.
    It's convenient very much using for indicating the app running status.
*/
#pragma once
#include <esp_netif.h>

esp_err_t WiFi_STA_Init();
esp_err_t WiFi_STA_Deinit();

// pass NULL to free
esp_err_t WiFi_STA_SetConfig(const char *pszSPL);

esp_err_t WiFi_STA_Start();
esp_err_t WiFi_STA_Stop();

esp_err_t WiFi_STA_Connect(bool bAutoReconnect);
esp_err_t WiFi_STA_Disconnect();

esp_err_t WiFi_STA_GetIP(esp_netif_ip_info_t *pIP);
bool WiFi_STA_IsIPConnected(void);

typedef void (*WiFi_STA_OnIPChanged)(esp_netif_ip_info_t *pIP);
void WiFi_STA_Set_OnIPChanged(WiFi_STA_OnIPChanged onIPChanged);