# BlueSetupWifi
Setup wifi connection parameters via BLE GATT service

## GATT
The uuid of GATT service is `54bc86d0-6e0c-47ac-9200-7a3e16c2a980`

### Characteristic SSID
+ uuid `54bc86d0-6e0c-47ac-9300-7a3e16c2a981`
+ READ and WRITE
+ value type zero-end `string`
+ max length `511` chars, auto padding 0 to end

This characteristic is used for carrying SSID and password, for example,
using '\n' to separate each segment. `SSID1\nPassword1\nSSID2\nPassword2`

### Characteristic IPv4
+ uuid `54bc86d0-6e0c-47ac-9300-7a3e16c2a982`
+ READ only
+ value type 32bits `unsigned int`

This characteristic is used for reporting IPv4 address.

## Android Mobile App Sample
There is an Android app sample [BlueWiFi](../../J1HeavyOS/BlueWiFi) writting in [Xamarin.Android](https://docs.microsoft.com/en-us/xamarin/android)

## sdkconfig
```
# require bluetooth nimble stack
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
```

## Sample
```C
#include "BlueSetupWiFi.h"

static void _on_bt_changed(BlueSetupWiFiLink link)
{
    // link can be one of:
    // BlueSetupWiFi_Link_STOP
    // BlueSetupWiFi_Link_ADVERTISING
    // BlueSetupWiFi_Link_CONNECTED
}

void app_main(){

    BlueSetupWiFi_Set_OnStatusChanged(_on_bt_changed);

    // true to init nvs internally
    BlueSetupWiFi_Init(true);
    BlueSetupWiFi_Start("Your-BLE-Device-Name");

    // GATT client can read `IPv4` characteristic
    // 192.168.0.1
    BlueSetupWiFi_SetIPv4(htonl(0xC0A80001));

    // GATT client can read/write `SSID` characteristic
    // the value will be saved in nvs
    const char *szSSID = BlueSetupWiFi_NVS_Read();

    // shutdown
    BlueSetupWiFi_Stop();
    // true to denint nvs internally
    BlueSetupWiFi_Deinit(true);
}
```