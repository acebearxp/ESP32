#pragma once

#include <stdio.h>
#include <esp_err.h>

esp_err_t GATT_NimBLE_init(void);
esp_err_t GATT_NimBLE_deinit(void);