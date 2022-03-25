#include "cs100a.h"
#include <esp_log.h>

typedef struct _CS100A_data{
    CS100A_Config_t config;
} CS1100A_data_t;

static CS1100A_data_t s_data;

esp_err_t CS100A_Init(const CS100A_Config_t *pConfig)
{
    s_data.config = *pConfig;

    rmt_config_t cfgRx = RMT_DEFAULT_CONFIG_RX(pConfig->rx.gpio, pConfig->rx.ch);
    /* APB(80MHz)->500kHz
       CS100A最大返回33ms左右的高电平,这样500kHz最大对应计数值16500
       以免溢出RMT计数单元(15bits)*/
    cfgRx.clk_div = 160;
    ESP_ERROR_CHECK(rmt_config(&cfgRx));
    ESP_ERROR_CHECK(rmt_driver_install(pConfig->rx.ch, 32, 0));

    rmt_config_t cfgTx = RMT_DEFAULT_CONFIG_TX(pConfig->tx.gpio, pConfig->tx.ch);
    ESP_ERROR_CHECK(rmt_config(&cfgTx));
    ESP_ERROR_CHECK(rmt_driver_install(pConfig->tx.ch, 0, 0));

    return ESP_OK;
}

esp_err_t CS100A_Deinit(void)
{
    ESP_ERROR_CHECK(rmt_driver_uninstall(s_data.config.tx.ch));
    ESP_ERROR_CHECK(rmt_driver_uninstall(s_data.config.rx.ch));

    return ESP_OK;
}

uint16_t CS100A_Ping(void)
{
    uint16_t u16usTim = 0;    
    uint32_t u32Clk;
    ESP_ERROR_CHECK(rmt_get_counter_clock(s_data.config.tx.ch, &u32Clk));

    rmt_item32_t data = {
        .duration0 = u32Clk * CONFIG_CS100A_TRIG_US  / 1000000,
        .level0 = 1,
        .duration1 = 0,
        .level1 = 0
    };

    RingbufHandle_t hRing;
        ESP_ERROR_CHECK(rmt_get_ringbuf_handle(s_data.config.rx.ch, &hRing));
    ESP_ERROR_CHECK(rmt_rx_start(s_data.config.rx.ch, true));

    ESP_ERROR_CHECK(rmt_write_items(s_data.config.tx.ch, &data, 1, false));

    size_t size;
    rmt_item32_t *pItems = xRingbufferReceive(hRing, &size, pdMS_TO_TICKS(CONFIG_CS100A_ECHO_MAX_MS));
    // ESP_LOGI("CS100A", "rx %u 0x%08x", size, (uint)pItems);
    ESP_ERROR_CHECK(rmt_rx_stop(s_data.config.rx.ch));
    if(pItems!=NULL && size > 0){
        ESP_ERROR_CHECK(rmt_get_counter_clock(s_data.config.rx.ch, &u32Clk));
        
        for(size_t i=0;i<size/sizeof(rmt_item32_t);i++){
            if(pItems[i].level0 > 0){
                u16usTim = (1000000/u32Clk)*pItems[i].duration0;
                ESP_LOGI("CS100A", "ping %uus", u16usTim);
            }
            if(pItems[i].level1 > 0){
                u16usTim = (1000000/u32Clk)*pItems[i].duration0;
                ESP_LOGI("CS100A", "%uus", u16usTim);
            }
        }
        vRingbufferReturnItem(hRing, pItems);
    }

    return u16usTim;
}