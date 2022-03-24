#pragma once
#include <esp_err.h>
#include <esp_event.h>

// 按下板载BOOT按钮激活蓝牙
void init_onboard_btn(esp_event_loop_handle_t hEventLoop, bool bActiveBLE);

// 红外遥控器激活蓝牙
void ir_btn_active_ble(void);

// 蓝牙广播
void StartBleAdv(void);
void StopBleAdv(void);