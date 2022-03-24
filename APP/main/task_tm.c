#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <lwip/sockets.h>
#include "task_tm.h"

#define UDP_PORT (8323)

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
    int sock = (int)pArgs;

    struct sockaddr_in remoteAddr;
    socklen_t remoteAddrLen;
    memset(&remoteAddr, 0, sizeof(remoteAddr));
    remoteAddr.sin_len = sizeof(remoteAddr);
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    remoteAddr.sin_port = htons(UDP_PORT);

    char buf[256];
    while(true){
        uint8_t u8 = recvfrom(sock, buf, 255, 0, (struct sockaddr*)&remoteAddr, &remoteAddrLen);
        buf[u8] = 0;
        ESP_LOGI("task_udp_recv", "recv: %s\n", buf);
    }
}

void task_udp_test(void *pArgs)
{
    char szH[] = "Hello from AceBear-ESP32S3";
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if(sock < 0){
        ESP_LOGE("udp_test", "socket failed...");
    }
    struct sockaddr_in srcAddr;
    memset(&srcAddr, 0, sizeof(srcAddr));
    srcAddr.sin_len = sizeof(srcAddr);
    srcAddr.sin_family = AF_INET;
    srcAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srcAddr.sin_port = htons(UDP_PORT);
    assert(bind(sock, (struct sockaddr*)&srcAddr, sizeof(srcAddr)) == 0);

    // 10.0.11.50
    struct sockaddr_in destAddr;
    memset(&srcAddr, 0, sizeof(srcAddr));
    destAddr.sin_len = sizeof(srcAddr);
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = inet_addr("10.0.11.50");
    destAddr.sin_port = htons(UDP_PORT);

    assert(xTaskCreate(task_udp_recv, "udp_recv", 4096, (void*)sock, 3, 0) == pdPASS);

    while(true){     
        char szTm[64];
        get_tm(szTm, sizeof(szTm));

        char buf[256];
        sprintf(buf, "%s [%s]\n", szH, szTm); 

        short s16Sent = sendto(sock, buf, strlen(buf)+1, 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
        if(s16Sent < 0){
            ESP_LOGW("task_udp_test", "sent failed!");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}