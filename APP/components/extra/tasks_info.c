#include "tasks_info.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static char state2char(eTaskState state)
{
    char map[] = "XRBSDE";
    return map[state];
}

void LogTasksInfo(const char *szTAG)
{
    BaseType_t u32Count = uxTaskGetNumberOfTasks();
    TaskStatus_t *pTasksInfo = malloc(sizeof(TaskStatus_t) * u32Count);
    u32Count = uxTaskGetSystemState(pTasksInfo, u32Count, NULL);

    // sort by task number
    BaseType_t *pTasksIdx = malloc(sizeof(BaseType_t)*u32Count);
    BaseType_t taskNumberMin;
    for(BaseType_t i=0;i<u32Count;i++){
        BaseType_t taskNumberMax = 0x7FFFFFF;
        taskNumberMin = i==0?0:pTasksInfo[pTasksIdx[i-1]].xTaskNumber;
        for(BaseType_t j=0;j<u32Count;j++){
            if(pTasksInfo[j].xTaskNumber > taskNumberMin && pTasksInfo[j].xTaskNumber < taskNumberMax){
                pTasksIdx[i] = j;
                taskNumberMax = pTasksInfo[j].xTaskNumber;
            }
        }
    }

    ESP_LOGI(szTAG, "%-7s %-12s%8s%8s%8s%6s", "#", "name","priority", "stk_wm", "stack", "state");
    uint32_t u32StackTotal = 0;
    for(BaseType_t i=0;i<u32Count;i++){
        TaskStatus_t *pInfo = &pTasksInfo[pTasksIdx[i]];
        uint uStackSize = heap_caps_get_allocated_size(pInfo->pxStackBase);
        u32StackTotal += uStackSize;
        ESP_LOGI(szTAG, "Task#%-2u %-16s%4u%8u%8u%4c",
            pInfo->xTaskNumber,
            pInfo->pcTaskName,
            pInfo->uxCurrentPriority,
            pInfo->usStackHighWaterMark,
            uStackSize,
            state2char(pInfo->eCurrentState));
    }

    uint32_t u32FreeSizeTotal = esp_get_free_heap_size();
    uint32_t u32FreeSizeSRAM = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(szTAG, "=== total %u tasks, total stack size %uk ===", u32Count, u32StackTotal>>10);
    ESP_LOGI(szTAG, "=== total free heap size %uk, internal free heap size %uk ===",
        u32FreeSizeTotal>>10, u32FreeSizeSRAM>>10);

    free(pTasksIdx);
    free(pTasksInfo);
}