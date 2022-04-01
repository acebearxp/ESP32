#include <esp_err.h>
#include <esp_event.h>
#include "IrRC.h"

esp_err_t start_uart(esp_event_loop_handle_t hEventLoop, IrRC_OnData on_ir_data);