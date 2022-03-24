#include "BlinkStatus.h"

void blink_init(esp_event_loop_handle_t hEventLoop);
void blink_set(BlinkStatus status);
void blink_auto(void);
