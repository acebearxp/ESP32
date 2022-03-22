#pragma once
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/rmt.h>

typedef void* SK68xx_handler_t;

SK68xx_handler_t sk68xx_driver_install(gpio_num_t gpio, rmt_channel_t rmtTx, uint16_t u16Count);
void sk68xx_driver_uninstall(SK68xx_handler_t hSK68xx);

esp_err_t sk68xx_write(SK68xx_handler_t hSK68xx, uint8_t *pRGB, uint16_t u16Size, uint16_t u16WaitMS);