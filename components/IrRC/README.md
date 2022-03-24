# IrRC
IR remote controller receiver

## sample
```C
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "IrRC.h"

#define GPIO_IR 4
#define RMT_TX 7
#define IR_PRIORITY 5

void on_ir_data(gpio_num_t gpio, uint16_t u16Address, uint8_t u8Code, void *pArgs)
{
    ESP_LOGI("IR_Demo", "Received IR Data: %u %u", u16Address u8Code);
}

void app_main(void)
{
    IrRC_Init(NULL, GPIO_IR, RMT_TX, IR_PRIORITY);
    IrRC_Set_OnData(GPIO_IR, on_ir_data, 0);

    while(true){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    IrRC_Deinit(GPIO_IR);
}
```