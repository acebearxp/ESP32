# ESP32
ESP32的试验性项目.

## 硬件环境
这些代码可以运行在官方的[ESP32-S3-DevKitC-1 (v1.0)](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html)开发板上.

这块开发板搭载了`ESP32-S3-WROOM-1-N8R8`(with 8M flash and 8M PSRAM)模块. 请参阅数据手册 https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_cn.pdf

![ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/_images/esp32-s3-devkitc-1-v1-isometric.png)

这些代码应该也适用于ESP32系列的其它芯片,但没有经过测试.

## 软件环境
依赖于 `乐鑫物联网框架` [esp-idf framework](https://github.com/espressif/esp-idf). 当前使用的是v4.4版本.

## components
+ [SK68xx](./components/SK68xx) RGB-LED driver.  
  There's a built-in RGB-LED on dev-board [ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html) connecting to `GPIO48`.
  The mode of RGB-LED is [SK6812-MINI-HS](https://www.rose-lighting.com/products/digital-full-color-mini-hs-sk6812-3535-rgb-smd-pixel-led-chip-dc5v) made by [Rose-Lighting](https://www.rose-lighting.com)
+ [BlinkStatus](./components/BlinkStatus) Using RGB-LED blinking to indicate the app status.
+ [BlueSetupWifi](./components/BlueSetupWifi) Setup wifi connection parameters via BLE.
+ [WiFiSTA](./components/WiFiSTA) Connect WiFi AP, with auto-reconnecting, and lwIP protocol
+ [IrRC](./components/IrRC) Ir remote controller receiver