#include "csro_common.h"
#include "esp_smartconfig.h"

static EventGroupHandle_t  wifi_event_group;
static const EventBits_t   CONNECTED_BIT       = BIT0;
static const EventBits_t   ESPTOUCH_DONE_BIT   = BIT1;

static void smartconfig_callback(smartconfig_status_t status, void *pdata)
{
    if (status == SC_STATUS_LINK) {            
        wifi_config_t *wifi_config = pdata;
        strcpy(system_info.router_ssid, "Jupiter");
        strcpy(system_info.router_pass, "150933205");
        debug("SSID: %s.\n", wifi_config->sta.ssid);
        debug("PASSWORD: %s.\n", wifi_config->sta.password);
        esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config);
        esp_wifi_connect();
    }
    else if (status == SC_STATUS_LINK_OVER) {
        xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}


static void smartconfig_task(void * pvParameters)
{
    EventBits_t uxBits;
    esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
    esp_smartconfig_start(smartconfig_callback);
    while (1) 
    {
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); 
        if(uxBits & ESPTOUCH_DONE_BIT) 
        {
            esp_smartconfig_stop();
            nvs_handle handle;
            nvs_open("system_info", NVS_READWRITE, &handle);
            nvs_set_str(handle, "ssid", system_info.router_ssid);
            nvs_set_str(handle, "pass", system_info.router_pass);
            nvs_set_u8(handle, "router_flag", 1);
            nvs_commit(handle);
            nvs_close(handle);
            esp_restart();
        }
    }
    vTaskDelete(NULL);
}


static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    if (event->event_id == SYSTEM_EVENT_STA_START) {
        xTaskCreate(smartconfig_task, "smartconfig_task", 2048, NULL, 5, NULL);
    }
    else if (event->event_id == SYSTEM_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
    else if (event->event_id == SYSTEM_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    }
    return ESP_OK;
}


void csro_task_smartconfig(void *pvParameters)
{
    csro_system_set_status(SMARTCONFIG);
    wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    esp_event_loop_init(wifi_event_handler, NULL);
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&config);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    while(true)
    {
        vTaskDelay(1000 / portTICK_RATE_MS);
        debug("csro_task_smartconfig. free heap %d.\n", esp_get_free_heap_size());
    }
    vTaskDelete(NULL);
}