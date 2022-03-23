#include "SK68xx.h"
#include <esp_log.h>

typedef struct _SK68xx_block{
    rmt_channel_t rmtTx;
    rmt_item32_t rmt_sample[2];
    uint16_t u16Count;
    uint8_t dataGRB[0]; // dynamic extended
} SK68xx_block_t;

static struct _SK68xx_data {
    uint16_t u16bit0[2];
    uint16_t u16bit1[2];
    uint16_t u16rst;
    SK68xx_block_t *pBlock[RMT_CHANNEL_MAX/2]; // half of them are tx channels
} s_data = {
        .u16bit0 = { CONFIG_SK68XX_T0HI_NANO_SECONDS, CONFIG_SK68XX_T0LO_NANO_SECONDS },
        .u16bit1 = { CONFIG_SK68XX_T1HI_NANO_SECONDS, CONFIG_SK68XX_T1LO_NANO_SECONDS },
        .u16rst = CONFIG_SK68XX_TRST_NANO_SECONDS
    };

static void IRAM_ATTR _ws2812_rmt_adapter(const void *pGRBData, rmt_item32_t *pRMTData,
                                         size_t u32RGBSize, size_t u32RMTSize,
                                         size_t *u32RGBTotalRead, size_t *u32RMTTotalWritten)
{
    /*
        RGBData is read from pGRBData byte by byte
        convert to RMTData bits by bits
    */
    SK68xx_block_t *pBlock;
    ESP_ERROR_CHECK(rmt_translator_get_context(u32RMTTotalWritten, (void**)(&pBlock)));

    uint32_t u32RGBRead = 0, u32WrittenBits = 0;
    uint8_t *pu8Src = (uint8_t*)pGRBData;
    rmt_item32_t *pRMTDst = pRMTData;
    while(u32RGBRead < u32RGBSize && u32WrittenBits < u32RMTSize){
        for(uint8_t i = 0; i < 8; i++){
            if((*pu8Src) & ( 1 << (7-i))){ // MSB first
                pRMTDst->val = pBlock->rmt_sample[1].val;
            }
            else{
                pRMTDst->val = pBlock->rmt_sample[0].val;
            }
            u32WrittenBits++;
            pRMTDst++;
        }
        u32RGBRead++;
        pu8Src++;
    }
    *u32RGBTotalRead = u32RGBRead;
    *u32RMTTotalWritten = u32WrittenBits;
}

SK68xx_handler_t sk68xx_driver_install(gpio_num_t gpio, rmt_channel_t rmtTx, uint16_t u16Count)
{
    assert(rmtTx < RMT_CHANNEL_MAX/2);
    assert(s_data.pBlock[rmtTx] == NULL);

    rmt_config_t cfg = RMT_DEFAULT_CONFIG_TX(gpio, rmtTx);
    cfg.clk_div = 2; // APB@80MHz -> 40MHz

    ESP_ERROR_CHECK(rmt_config(&cfg));
    ESP_ERROR_CHECK(rmt_driver_install(rmtTx, 0, 0));

    uint32_t u32Clk;
    ESP_ERROR_CHECK(rmt_get_counter_clock(rmtTx, &u32Clk));
    float fRatio = u32Clk / 1e9f;

    SK68xx_block_t *pBlock = malloc(sizeof(SK68xx_block_t) + 3*u16Count);
    pBlock->rmtTx = rmtTx;
    pBlock->rmt_sample[0].duration0 = fRatio * s_data.u16bit0[0];
    pBlock->rmt_sample[0].level0 = 1;
    pBlock->rmt_sample[0].duration1 = fRatio * s_data.u16bit0[1];
    pBlock->rmt_sample[0].level1 = 0;
    pBlock->rmt_sample[1].duration0 = fRatio * s_data.u16bit1[0];
    pBlock->rmt_sample[1].level0 = 1;
    pBlock->rmt_sample[1].duration1 = fRatio * s_data.u16bit1[1];
    pBlock->rmt_sample[1].level1 = 0;
    pBlock->u16Count = u16Count;
    s_data.pBlock[rmtTx] = pBlock;
  
    ESP_ERROR_CHECK(rmt_translator_init(rmtTx, _ws2812_rmt_adapter));
    ESP_ERROR_CHECK(rmt_translator_set_context(rmtTx, pBlock));

    return pBlock;
}

void sk68xx_driver_uninstall(SK68xx_handler_t hSK68xx)
{
    SK68xx_block_t *pBlock = hSK68xx;
    rmt_channel_t rmtTx = pBlock->rmtTx;
    ESP_ERROR_CHECK(rmt_driver_uninstall(rmtTx));
    free(pBlock);
    s_data.pBlock[rmtTx] = NULL;
}

esp_err_t sk68xx_write(SK68xx_handler_t hSK68xx, uint8_t *pRGB, uint16_t u16SizeInBytes, uint16_t u16WaitMS)
{
    SK68xx_block_t *pBlock = hSK68xx;
    assert(u16SizeInBytes <= pBlock->u16Count*3);

    // RGB -> GRB
    for(uint16_t i=0;i+2<u16SizeInBytes;i+=3){
        pBlock->dataGRB[i + 0] = pRGB[i + 1];
        pBlock->dataGRB[i + 1] = pRGB[i + 0];
        pBlock->dataGRB[i + 2] = pRGB[i + 2];
    }

    ESP_ERROR_CHECK(rmt_write_sample(pBlock->rmtTx, pBlock->dataGRB, u16SizeInBytes, false));
    return rmt_wait_tx_done(pBlock->rmtTx, pdMS_TO_TICKS(u16WaitMS));
}