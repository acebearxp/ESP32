#pragma once

#include <esp_err.h>

// 按下板载BOOT按钮激活蓝牙
void boot_btn_active_ble(void);


// 红外遥控器激活蓝牙
void ir_btn_active_ble(void);

// 蓝牙广播
void StartBleAdv(uint8_t u8Flag);
void StopBleAdv(void);