# BlinkStatus
Using RGB-LED blinking to indicate the app status.

Blinking mode is pre-defined in `BlinkStatusEnum`

Internally each blinking mode using 2 RGB colors,
each color's blinking pattern controlled by 8 bits

Each mode can adjust the blinking period separately.

Blinking timing is controlled by a hardware timer of timer group.

## Sample
```C
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_check.h>
#include "BlinkStatus.h"

#define BLINKING_TIMER_GROUP 0
#define BLINKING_TIMER_INDEX 1

void app_main(void)
{
    ESP_ERROR_CHECK(BlinkStatus_Init(BLINKING_TIMER_GROUP, BLINKING_TIMER_INDEX));

    while(1){
        BlinkStatus_Set(BLINK_STATUS_POWER_ON);
        vTaskDelay(500);
        BlinkStatus_Set(BLINK_STATUS_STANDBY);
        vTaskDelay(500);
    }

    ESP_ERROR_CHECK(BlinkStatus_Uninit());
}
```