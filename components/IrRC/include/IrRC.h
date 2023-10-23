#pragma once
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <driver/gpio.h>
#include <driver/rmt_rx.h>

esp_err_t IrRC_Init(esp_event_loop_handle_t hEventLoop, gpio_num_t gpio);
esp_err_t IrRC_Deinit(gpio_num_t gpio);

typedef void (*IrRC_OnData)(gpio_num_t gpio, uint16_t u16Address, uint8_t u8Code, void *pArgs);
esp_err_t IrRC_Set_OnData(gpio_num_t gpio, IrRC_OnData ondata, void *pArgs);