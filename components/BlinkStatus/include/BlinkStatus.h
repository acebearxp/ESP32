#pragma once

#include <esp_err.h>
#include <esp_event.h>

typedef enum BlinkStatusEnum{
    BLINK_STATUS_NA           = 0x00, // NotAvailable
    BLINK_STATUS_POWER_ON     = 0x01, // power on, tasks yet not  to running
    BLINK_STATUS_BT_STANDBY   = 0x02, // bluetooth standby
    BLINK_STATUS_BT_CONN      = 0x03, // bluetooth connected
    BLINK_STATUS_BTWF_STANDBY = 0x04, // bluetooth and wifi both standby
    BLINK_STATUS_WIFI_STANDBY = 0X05  // wifi standby
} BlinkStatus;

/** initialize the BlinkStatus
 *  @param hEventLoop The event loop handler that drive the blinking
 *  @return ESP_OK for success
 */
esp_err_t BlinkStatus_Init(esp_event_loop_handle_t hEventLoop);
esp_err_t BlinkStatus_Uninit();

uint8_t BlinkStatus_Get();
void BlinkStatus_Set(BlinkStatus status);