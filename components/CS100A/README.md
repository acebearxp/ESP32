# CS100A
超声测距CS100A [岸歌传感](http://www.100sensor.com)

![](http://www.100sensor.com/assets/img/us025.jpg)

# sample
```C
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "cs100a.h"

void app_main(void)
{
    CS100A_Config_t cs100_cfg = {
        .tx = { RMT_CHANNEL_0, GPIO_NUM_1 },
        .rx = { RMT_CHANNEL_4, GPIO_NUM_2 }
    };
    ESP_ERROR_CHECK(CS100A_Init(&cs100_cfg));

    while(true){
        uint16_t u16TimeUS = CS100A_Ping();
        float fRange = 0.5f * 0.34f * u16TimeUS;
        ESP_LOGI("CS100A", "%.0fmm", fRange);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(CS100A_Deinit());
}
```