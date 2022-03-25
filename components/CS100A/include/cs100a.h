#pragma once
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/rmt.h>

typedef struct _CS100A_Endpoint{
    rmt_channel_t ch;
    gpio_num_t gpio;
} CS100A_Endpoint_t;

typedef struct _CS100A_Config{
    CS100A_Endpoint_t tx; // 发射单元
    CS100A_Endpoint_t rx; // 接收单元
} CS100A_Config_t;

/** 初始化CS100A超声测距模块
 * @param pConfig 指定收发单元的GPIO和使用的RMT通道
 * @return 成功返回ESP_OK
 * @remark 内部使用RMT单元
 *         发射单元连接TRIG,需要使用发射RMT信道RMT_CHANNEL_0~3
 *         接收单元连接ECHO,需要使用接收RMT信道RMT_CHANNEL_4~7
 */
esp_err_t CS100A_Init(const CS100A_Config_t *pConfig);

/** 反初始化CS100A超声测距模块
 *  @return 成功返回ESP_OK 
 */
esp_err_t CS100A_Deinit(void);

/** 执行一次超声波测距
 *  @return 超声回波的往返时间,微秒
 *  @remark 返回的时间(微秒)*声速(0.34毫米/微秒)/2 即是距离(毫米)
 */
uint16_t CS100A_Ping(void);