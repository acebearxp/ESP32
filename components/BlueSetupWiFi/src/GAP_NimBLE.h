#pragma once

#include <stdio.h>
#include <esp_err.h>

esp_err_t GAP_NimBLE_init(const char* szDevName);

int GAP_Advertising_start();
esp_err_t GAP_Advertising_stop();