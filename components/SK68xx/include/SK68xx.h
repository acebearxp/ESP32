#pragma once
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/rmt_tx.h>

// SK68xx handler
typedef void* SK68xx_handler_t;

/** install the driver for sk68xx
 *  @param gpio the gpio which sk68xx LED connected
 *  @param u16Count how many sk68xx chained together
 *  @return the driver handler
 *  @remark total 4 drivers can be installed at the same time, since there're 4 rmt sending channels in ESP32S3
 */
SK68xx_handler_t sk68xx_driver_install(gpio_num_t gpio, uint16_t u16Count);

/** uninstall the driver for sk68xx
 *  @param hSK68xx the driver handler
 */
void sk68xx_driver_uninstall(SK68xx_handler_t hSK68xx);

/** writing RGB data to drive the sk68xx led
 *  @param hSK68xx the driver handler
 *  @param pRGB the RGB data buffer, each led requires 3 bytes: red, green, blue in order.
 *  @param u16SizeInBytes the size of the RGB data buffer, in bytes, should be multiple of 3
 *  @param u16WaitMS the max time waiting date sending complete, in ms
 */
esp_err_t sk68xx_write(SK68xx_handler_t hSK68xx, uint8_t *pRGB, uint16_t u16SizeInBytes, uint16_t u16WaitMS);