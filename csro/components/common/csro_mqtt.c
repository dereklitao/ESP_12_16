#include "csro_common.h"








void csro_task_mqtt(void *pvParameters)
{
    while(true)
    {
        vTaskDelay(1000 / portTICK_RATE_MS);
        debug("csro_task_mqtt. free heap %d.\n", esp_get_free_heap_size());
    }
    vTaskDelete(NULL);
}