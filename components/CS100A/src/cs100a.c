#include "cs100a.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_rx.h>
#include <esp_log.h>

#define RMT_RX_BUF_SIZE 64
#define RMT_FREQ 500000

typedef struct _CS100A_data{
    CS100A_Config_t config;
    rmt_channel_handle_t hTx;
    rmt_channel_handle_t hRx;
    rmt_encoder_handle_t hEncoder;
    rmt_symbol_word_t bufRx[RMT_RX_BUF_SIZE];
    size_t sizeRx;
    TaskHandle_t hTaskPing;
} CS1100A_data_t;

static CS1100A_data_t s_data;

static bool IRAM_ATTR rmt_rx_cb(rmt_channel_handle_t hRX, const rmt_rx_done_event_data_t *pDataRecv, void *pEvData)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    CS1100A_data_t *pData = (CS1100A_data_t*)pEvData;
    pData->sizeRx = pDataRecv->num_symbols;
    vTaskNotifyGiveFromISR(pData->hTaskPing, &xHigherPriorityTaskWoken);
    return xHigherPriorityTaskWoken == pdTRUE;
}

esp_err_t CS100A_Init(const CS100A_Config_t *pConfig)
{
    s_data.config = *pConfig;

    /* APB(80MHz)->500kHz
       CS100A最大返回33ms左右的高电平,这样500kHz最大对应计数值16500
       以免溢出RMT计数单元(15bits)*/
    uint32_t u32Clk = RMT_FREQ;
    rmt_tx_channel_config_t txConfig = {
        .gpio_num = pConfig->tx,
        .clk_src = RMT_BASECLK_APB,
        .resolution_hz = u32Clk,
        .mem_block_symbols = 48,
        .trans_queue_depth = 4
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&txConfig, &s_data.hTx));
    
    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = {
            .duration0 = u32Clk * CONFIG_CS100A_TRIG_US  / 1000000,
            .level0 = 0,
            .duration1 = u32Clk * CONFIG_CS100A_TRIG_US  / 1000000,
            .level1 = 1,
        },
        .bit1 = {
            .duration0 = u32Clk * CONFIG_CS100A_TRIG_US  / 1000000,
            .level0 = 0,
            .duration1 = u32Clk * CONFIG_CS100A_TRIG_US * 3 / 1000000,
            .level1 = 1,
        },
        .flags.msb_first = true,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &s_data.hEncoder));
    ESP_ERROR_CHECK(rmt_enable(s_data.hTx));

    rmt_rx_channel_config_t rxConfig = {
        .gpio_num = pConfig->rx,
        .clk_src = RMT_CLK_SRC_APB, // APB@80MHz
        .resolution_hz = u32Clk,
        .mem_block_symbols = 48
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rxConfig, &s_data.hRx));
    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = rmt_rx_cb
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(s_data.hRx, &cbs, &s_data));
    ESP_ERROR_CHECK(rmt_enable(s_data.hRx));

    return ESP_OK;
}

esp_err_t CS100A_Deinit(void)
{
    ESP_ERROR_CHECK(rmt_disable(s_data.hTx));
    ESP_ERROR_CHECK(rmt_del_encoder(s_data.hEncoder));
    ESP_ERROR_CHECK(rmt_del_channel(s_data.hTx));

    ESP_ERROR_CHECK(rmt_disable(s_data.hRx));
    
    ESP_ERROR_CHECK(rmt_del_channel(s_data.hRx));

    return ESP_OK;
}

uint16_t CS100A_Ping(void)
{
    uint16_t u16usTim = 0;    
    uint32_t u32Clk = RMT_FREQ;

    s_data.hTaskPing = xTaskGetCurrentTaskHandle();

    rmt_receive_config_t recv_config = {
        .signal_range_min_ns = 1000, // 12.5us
        .signal_range_max_ns = 10 * 1000 * 1000 // 10ms
    };
    // non-blocking
    rmt_receive(s_data.hRx, s_data.bufRx, RMT_RX_BUF_SIZE, &recv_config);

    rmt_transmit_config_t cfg = {
        .loop_count = 0,
        .flags = {
            .eot_level = 0
        }
    };
    uint8_t txData = 0;
    ESP_ERROR_CHECK(rmt_transmit(s_data.hTx, s_data.hEncoder, &txData, 1, &cfg));

    // blocking wait
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_CS100A_ECHO_MAX_MS));
    if(s_data.sizeRx > 0){
        u16usTim = (1000000/u32Clk)*s_data.bufRx[0].duration0;
        ESP_LOGI("CS100A", "ping %uus", u16usTim);
    }

    return u16usTim;
}
