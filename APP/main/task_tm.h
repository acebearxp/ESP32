#pragma once
#include <esp_event.h>

void start_udp_test(esp_event_loop_handle_t hEventLoop);
void task_udp_recv(void *pArgs);

void trigger_udp_send(void);

bool get_tm(char *buf, uint8_t len);