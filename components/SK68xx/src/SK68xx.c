#include "SK68xx.h"
#include <esp_log.h>

typedef struct _SK68xx_block{
    rmt_channel_handle_t hRMT;
    rmt_encoder_handle_t hEncoder;
    struct _SK68xx_block *pNext;
    uint16_t u16Count;
    uint8_t dataGRB[0]; // dynamic extended
} SK68xx_block_t;

static struct _SK68xx_data {
    uint16_t u16bit0[2];
    uint16_t u16bit1[2];
    uint16_t u16rst;
    SK68xx_block_t *pData;
} s_data = {
        .u16bit0 = { CONFIG_SK68XX_T0HI_NANO_SECONDS, CONFIG_SK68XX_T0LO_NANO_SECONDS },
        .u16bit1 = { CONFIG_SK68XX_T1HI_NANO_SECONDS, CONFIG_SK68XX_T1LO_NANO_SECONDS },
        .u16rst = CONFIG_SK68XX_TRST_NANO_SECONDS,
        .pData = NULL
    };

/*static SK68xx_block_t* _search(rmt_channel_handle_t hRMT)
{
    struct _SK68xx_block *pBlock = s_data.pData;
    while(pBlock){
        if(pBlock->hRMT == hRMT) break;
        pBlock = pBlock->pNext;
    }
    return pBlock;
}*/

static SK68xx_block_t* _create(uint16_t u16Count)
{
    SK68xx_block_t *pNew = malloc(sizeof(SK68xx_block_t) + 3*u16Count);
    pNew->pNext = NULL;

    if(s_data.pData == NULL){
        s_data.pData = pNew;
    }
    else{
        SK68xx_block_t *pBlock = s_data.pData;
        while(pBlock->pNext) pBlock = pBlock->pNext;
        pBlock->pNext = pNew;
    }

    return pNew;
}

static void _destroy(SK68xx_block_t *pBlock)
{
    if(s_data.pData == pBlock){
        s_data.pData = pBlock->pNext;
    }
    else{
        SK68xx_block_t *pBefore = s_data.pData;
        while(pBefore->pNext && pBefore->pNext != pBlock) pBefore = pBefore->pNext;
        assert(pBefore->pNext == pBlock);
        pBefore->pNext = pBlock->pNext;
    }

    free(pBlock);
}

SK68xx_handler_t sk68xx_driver_install(gpio_num_t gpio, uint16_t u16Count)
{
    SK68xx_block_t *pBlock = _create(u16Count);
    pBlock->u16Count = u16Count;

    const uint32_t u32Clk = 40*1000*1000; // 40MHz

    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = gpio,
        .clk_src = RMT_CLK_SRC_APB, // APB@80MHz
        .resolution_hz = u32Clk, // 40MHz
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags = {
            .invert_out = false,
            .with_dma = true,
            .io_loop_back = false,
            .io_od_mode = false
        }
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &pBlock->hRMT));

    float fRatio = u32Clk / 1e9f;
    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = {
            .duration0 = fRatio * s_data.u16bit0[0],
            .level0 = 1,
            .duration1 = fRatio * s_data.u16bit0[1],
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = fRatio * s_data.u16bit1[0],
            .level0 = 1,
            .duration1 = fRatio * s_data.u16bit1[1],
            .level1 = 0,
        },
        .flags.msb_first = true,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &pBlock->hEncoder));

    ESP_ERROR_CHECK(rmt_enable(pBlock->hRMT));

    return pBlock;
}

void sk68xx_driver_uninstall(SK68xx_handler_t hSK68xx)
{
    SK68xx_block_t *pBlock = hSK68xx;
    ESP_ERROR_CHECK(rmt_disable(pBlock->hRMT));
    ESP_ERROR_CHECK(rmt_del_encoder(pBlock->hEncoder));
    ESP_ERROR_CHECK(rmt_del_channel(pBlock->hRMT));
    _destroy(pBlock);
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
    
    rmt_transmit_config_t cfg = {
        .loop_count = 0,
        .flags = {
            .eot_level = 0
        }
    };
    ESP_ERROR_CHECK(rmt_transmit(pBlock->hRMT, pBlock->hEncoder, pBlock->dataGRB, u16SizeInBytes, &cfg));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(pBlock->hRMT, u16WaitMS));
    return ESP_OK;
}