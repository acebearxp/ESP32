#pragma once
#include <freertos/FreeRTOS.h>

void LogTasksInfo(const char *szTAG, bool bTaskDetail);

float detectChipTemperature();