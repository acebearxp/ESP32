# SK68xx
[SK6805/SK6812](https://www.rose-lighting.com/products/digital-full-color-mini-hs-sk6812-3535-rgb-smd-pixel-led-chip-dc5v) RGB-LED driver.

This RGB-LED is compatible with WS2812B. The actual data transmission time please look for datasheet  
https://www.rose-lighting.com/wp-content/uploads/sites/53/2020/05/SK68XX-MINI-HS-REV.04-EN23535RGB-thick.pdf

This driver using RMT channel send data internally, each GPIO will binding to a RMT Tx channel.
Since ESP32S3 has 4 RMT Tx channels(0~4), this driver can be install 4 times for different (GPIO,RMT_Tx) pairs at the same time.

## Sample
```C
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "SK68xx.h"

#define LED_GPIO    48
#define LED_COUNT   1
#define LED_RMT_TX  2

void app_main(void)
{
    SK68xx_handler_t hSK68xx =
        sk68xx_driver_install(LED_GPIO, LED_RMT_TX, LED_COUNT);

    uint8_t colorRED[3]   = { 0x03, 0x03, 0x00 };
    uint8_t colorGREEN[3] = { 0x00, 0x03, 0x03 };
    while(1){
        sk68xx_write(hSK68xx, colorRED, 3, 50);
        vTaskDelay(50);
        sk68xx_write(hSK68xx, colorGREEN, 3, 50);
        vTaskDelay(50);
    }

    sk68xx_driver_uninstall(hSK68xx);
}
```