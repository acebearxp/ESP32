# WiFiSTA
Connect WiFi AP, with auto-reconnecting, and lwIP protocol

+ Multiple SSID/password supported
+ auto reconnect

## Sample
```C

static void _on_ip_changed(esp_netif_ip_info_t *pIP)
{
    if(pIP){
        // connected
    }
    else{
        // disconnected
    }
}

void app_main()
{
    char szWiFi="SSID1\npassword1\nSSID2\npassword2";

    ESP_ERROR_CHECK(WiFi_STA_Init());
    ESP_ERROR_CHECK(WiFi_STA_SetConfig(szWiFi));
    ESP_ERROR_CHECK(WiFi_STA_Start());
    // true to enable auto-reconnect
    ESP_ERROR_CHECK(WiFi_STA_Connect(true));

    esp_netif_ip_info_t info;
    WiFi_STA_GetIP(&info);
    ESP_LOGI(c_szTAG, "IPv4 %d.%d.%d.%d", IP2STR(&info.ip));

    // optional, only working when auto-reconnect is enabled
    WiFi_STA_Set_OnIPChanged(_on_ip_changed);

    // ...

    ESP_ERROR_CHECK(WiFi_STA_Disconnect());
    ESP_ERROR_CHECK(WiFi_STA_Stop());
    ESP_ERROR_CHECK(WiFi_STA_Deinit());
}
```