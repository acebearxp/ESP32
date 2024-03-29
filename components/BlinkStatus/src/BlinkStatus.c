#include "BlinkStatus.h"
#include <esp_check.h>
#include <esp_timer.h>
// #include <driver/timer.h>
#include <driver/gptimer.h>
#include <esp_log.h>
#include "SK68xx.h"

static const char c_szTAG[] = "BlinkStatus";

typedef struct _blinking_args_struct{
    uint8_t u8Color[6];     // blinking color RGB x 2
    uint8_t u8Schema[2];    // blinking schema
    uint8_t u8Freq;         // blinking frequency Hz
} blinking_args_t;

static struct _blinking_data_struct{
    bool bCreateEventLoop;
    esp_event_loop_handle_t hEventLoop;
    esp_event_handler_instance_t hOnEvent;
    SK68xx_handler_t hSK68xx;
#ifdef CONFIG_BLINK_STATUS_TIMER_ESP
    esp_timer_handle_t hESPTimer;
#endif
#ifdef CONFIG_BLINK_STATUS_TIMER_GPT
    gptimer_handle_t hGPTimer;
#endif
    uint8_t u8Mode;
    blinking_args_t blinking_mode[6];
} s_data ={
    .blinking_mode = {
        { { 0x00,0x00,0x00, 0x00,0x00,0x00 }, { 0xff, 0x00 }, 0x00 },
        { { 0x03,0x00,0x00, 0x00,0x00,0x00 }, { 0xaa, 0x55 }, 0x05 }, // POWER_ON red flash rapidly
        { { 0x00,0x00,0x03, 0x00,0x00,0x00 }, { 0xc0, 0x00 }, 0x05 }, // BT_STANDBY blue flash slowly
        { { 0x00,0x00,0x03, 0x00,0x00,0x50 }, { 0xa4, 0x04 }, 0x04 }, // BT_CONN blue flash 1-1-2
        { { 0x00,0x09,0x00, 0x00,0x00,0x03 }, { 0xc8, 0x30 }, 0x04 }, // BTWF_STANDBY green blue flash slowly
        { { 0x00,0x09,0x00, 0x00,0x00,0x00 }, { 0xc8, 0x30 }, 0x04 }  // WIFI_STANDBY green flash slowly
    }
};

typedef enum _blink_status_event{
    BLINK_STATUS_EVENT_TICK,
    BLINK_STATUS_EVENT_MODE_CHANGED
} blink_status_event_t;

ESP_EVENT_DEFINE_BASE(BLINK_STATUS_EVENT);

static uint8_t _comp_color(uint8_t u8Pos, uint8_t *color)
{
    uint8_t u8Mask[2]; // 2 colors
    blinking_args_t *pMode = &s_data.blinking_mode[s_data.u8Mode];
    for(uint8_t i=0;i<2;i++) u8Mask[i] = ((pMode->u8Schema[i] >> u8Pos) & 0x01) ? 0xff : 0x00;

    // for color 1
    for(uint8_t i=0;i<3;i++) color[i] = pMode->u8Color[i] & u8Mask[0];
    // for color 2
    for(uint8_t i=0;i<3;i++) color[i] += pMode->u8Color[i+3] & u8Mask[1];

    return (u8Mask[0]&0x02)|(u8Mask[1]%0x01);
}

static void on_event(void *pArgs, esp_event_base_t ev, int32_t evid, void *pData)
{
    static uint8_t s_u8Pos = 0, s_u8MaskLast = 0xff;
    uint8_t u8Mask;
    uint8_t color[3]; // RGB

    switch(evid){
        case BLINK_STATUS_EVENT_MODE_CHANGED:
            s_u8Pos = 0;
            s_u8MaskLast = _comp_color(s_u8Pos, color);
            sk68xx_write(s_data.hSK68xx, color, 3, 50);
            break;
        default:
            s_u8Pos = (s_u8Pos + 1) % 8;
            u8Mask = _comp_color(s_u8Pos, color);
            if(u8Mask != s_u8MaskLast){
                // color changed
                sk68xx_write(s_data.hSK68xx, color, 3, 50);
                s_u8MaskLast = u8Mask;
            }
    }
}

#ifdef CONFIG_BLINK_STATUS_TIMER_ESP
static void on_timer(void *pArgs)
{
    esp_event_post_to(s_data.hEventLoop, BLINK_STATUS_EVENT, BLINK_STATUS_EVENT_TICK, NULL, 0, 0);
}

static void _start_timer(uint8_t u8Freq)
{
    uint64_t period = 1000000/u8Freq;
    esp_timer_start_periodic(s_data.hESPTimer, period);
}

static void _stop_timer(void)
{
    esp_timer_stop(s_data.hESPTimer);
}

static esp_err_t _init_timer(void)
{
    esp_timer_create_args_t timer_args = {
        .callback = on_timer,
        .arg = &s_data,
        .dispatch_method = ESP_TIMER_TASK,
        .name = c_szTAG,
        .skip_unhandled_events = false
    };
    return esp_timer_create(&timer_args, &s_data.hESPTimer);
}

static esp_err_t _uninit_timer(void)
{
    _stop_timer();
    return esp_timer_delete(s_data.hESPTimer);
}
#endif

#ifdef CONFIG_BLINK_STATUS_TIMER_GPT
static bool IRAM_ATTR on_isr_tim(gptimer_handle_t hGPTimer, const gptimer_alarm_event_data_t *pevData, void *pArgs)
{
    BaseType_t pxHigherPriorityTaskWoken;
    esp_event_isr_post_to(s_data.hEventLoop, BLINK_STATUS_EVENT, BLINK_STATUS_EVENT_TICK, NULL, 0, &pxHigherPriorityTaskWoken);
    return pxHigherPriorityTaskWoken == pdTRUE;
}

static void _start_timer(uint8_t u8Freq)
{
    uint32_t u32Hz = 1000; // 1kHz
    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count = u32Hz/u8Freq,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(s_data.hGPTimer, &alarm_cfg));
    ESP_ERROR_CHECK(gptimer_set_raw_count(s_data.hGPTimer, 0));
    ESP_ERROR_CHECK(gptimer_start(s_data.hGPTimer));
}

static void _stop_timer(void)
{
    ESP_ERROR_CHECK(gptimer_stop(s_data.hGPTimer));
}

static esp_err_t _init_timer(void)
{
    gptimer_config_t tim_cfg = {
        .clk_src = CONFIG_BLINK_STATUS_TIMER_GPT_CLK_SRC,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000 // 1kHz
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&tim_cfg, &(s_data.hGPTimer)));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = on_isr_tim
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_data.hGPTimer, &cbs, 0));
    ESP_ERROR_CHECK(gptimer_enable(s_data.hGPTimer));

    return ESP_OK;
}

static esp_err_t _uninit_timer(void)
{
    ESP_ERROR_CHECK(gptimer_disable(s_data.hGPTimer));
    return gptimer_del_timer(s_data.hGPTimer);
}
#endif

esp_err_t BlinkStatus_Init(esp_event_loop_handle_t hEventLoop)
{
    assert(s_data.hOnEvent == NULL);

    if(hEventLoop == NULL){
        s_data.bCreateEventLoop = true;
        esp_event_loop_args_t evLoopArgs = {
            .queue_size = 4,
            .task_name = c_szTAG,
            .task_priority = 5,
            .task_stack_size = 2048,
            .task_core_id = tskNO_AFFINITY
        };
        ESP_ERROR_CHECK(esp_event_loop_create(&evLoopArgs, &hEventLoop));
    }

    s_data.hEventLoop = hEventLoop;
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(hEventLoop,
        BLINK_STATUS_EVENT, ESP_EVENT_ANY_ID, on_event, &s_data, &s_data.hOnEvent));

    ESP_ERROR_CHECK(_init_timer());

    s_data.hSK68xx = sk68xx_driver_install(CONFIG_BLINK_STATUS_DIN_GPIO, 1);

    return ESP_OK;
}

esp_err_t BlinkStatus_Uninit()
{
    ESP_ERROR_CHECK(_uninit_timer());

    esp_err_t espRet = esp_event_handler_instance_unregister_with(s_data.hEventLoop, BLINK_STATUS_EVENT, ESP_EVENT_ANY_ID, s_data.hOnEvent);
    if(espRet == ESP_OK) s_data.hOnEvent = NULL;

    if(s_data.bCreateEventLoop){
        ESP_ERROR_CHECK(esp_event_loop_delete(s_data.hEventLoop));
    }

    return espRet;
}

BlinkStatus BlinkStatus_Get()
{
    return s_data.u8Mode;
}

void BlinkStatus_Set(BlinkStatus status)
{
    if(s_data.u8Mode == status) return;

    uint8_t u8FreqLast = s_data.blinking_mode[s_data.u8Mode].u8Freq;
    uint8_t u8FreqNext = s_data.blinking_mode[status].u8Freq;
    s_data.u8Mode = status;

    // need to reset timer
    if(u8FreqLast != u8FreqNext){
        if(u8FreqLast != 0) _stop_timer();
        if(u8FreqNext != 0) _start_timer(u8FreqNext);
    }

    esp_event_post_to(s_data.hEventLoop, BLINK_STATUS_EVENT, BLINK_STATUS_EVENT_MODE_CHANGED, NULL, 0, 0);
}