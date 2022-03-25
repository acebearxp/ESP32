#include "task_tm.h"
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_netif.h>
#include <lwip/sockets.h>

#define UDP_PORT (8323)

typedef enum _udp_test_event{
    UDP_TEST_EVENT_TX
} udp_test_event_t;

ESP_EVENT_DEFINE_BASE(UDP_TEST_EVENT);

typedef struct _udp_test_data{
    int sock;
    struct sockaddr_in srcAddr;
    struct sockaddr_in destAddr;
    esp_timer_handle_t hTimer;
    esp_event_loop_handle_t hEventLoop;
    esp_event_handler_instance_t hOnEvent;
} udp_task_data_t;

static udp_task_data_t s_data;

bool get_tm(char *buf, uint8_t len)
{
    time_t utcNow;
    struct tm tmInfo;
    time(&utcNow);
    localtime_r(&utcNow, &tmInfo);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S(%z)", &tmInfo);

    return ((unsigned int)utcNow) > 1e9;
}

void task_udp_recv(void *pArgs)
{
    udp_task_data_t *pData = (udp_task_data_t*)pArgs;

    struct sockaddr_in remoteAddr;
    socklen_t remoteAddrLen;
    memset(&remoteAddr, 0, sizeof(remoteAddr));
    remoteAddr.sin_len = sizeof(remoteAddr);
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    remoteAddr.sin_port = htons(UDP_PORT);

    char buf[256];
    while(true){
        uint8_t u8 = recvfrom(pData->sock, buf, 255, 0, (struct sockaddr*)&remoteAddr, &remoteAddrLen);
        buf[u8] = 0;
        ESP_LOGI("task_udp_recv", "%d.%d.%d.%d: %s\n", IP2STR((struct esp_ip4_addr*)&remoteAddr.sin_addr), buf);
    }
}

static void on_event(void *pArgs, esp_event_base_t ev, int32_t evid, void *pEvData)
{
    udp_task_data_t *pData = (udp_task_data_t*)pArgs;

    char szTm[64];
    get_tm(szTm, sizeof(szTm));

    char buf[256];
    sprintf(buf, "%s [%s]\n", "Hello from AceBear-ESP32S3", szTm); 

    short s16Sent = sendto(pData->sock, buf, strlen(buf)+1, 0, (struct sockaddr*)&pData->destAddr, sizeof(pData->destAddr));
    if(s16Sent < 0){
        ESP_LOGW("task_udp_test", "sent failed!");
    }
}

void on_timer(void *pArgs)
{
    udp_task_data_t *pData = (udp_task_data_t*)pArgs;
    esp_event_post_to(pData->hEventLoop, UDP_TEST_EVENT, UDP_TEST_EVENT_TX, NULL, 0, 0);
}

void start_udp_test(esp_event_loop_handle_t hEventLoop)
{
    s_data.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if(s_data.sock < 0){
        ESP_LOGE("udp_test", "socket failed...");
        return;
    }

    s_data.srcAddr.sin_len = sizeof(struct sockaddr_in);
    s_data.srcAddr.sin_family = AF_INET;
    s_data.srcAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_data.srcAddr.sin_port = htons(UDP_PORT);
    assert(bind(s_data.sock, (struct sockaddr*)&s_data.srcAddr, s_data.srcAddr.sin_len) == 0);

    // 10.0.11.50
    s_data.destAddr.sin_len = sizeof(struct sockaddr_in);
    s_data.destAddr.sin_family = AF_INET;
    s_data.destAddr.sin_addr.s_addr = inet_addr("10.0.11.50");
    s_data.destAddr.sin_port = htons(UDP_PORT);

    assert(xTaskCreate(task_udp_recv, "udp_recv", 4096, &s_data, 16, 0) == pdPASS);

    s_data.hEventLoop = hEventLoop;
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        hEventLoop, UDP_TEST_EVENT, ESP_EVENT_ANY_ID, on_event, &s_data, &s_data.hOnEvent));

    /*esp_timer_create_args_t args = {
        .callback = on_timer,
        .arg = &s_data,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "udp_send_test",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_data.hTimer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_data.hTimer, 1000000*5));*/
}

void trigger_udp_send(void)
{
    esp_event_post_to(s_data.hEventLoop, UDP_TEST_EVENT, UDP_TEST_EVENT_TX, NULL, 0, 0);
}