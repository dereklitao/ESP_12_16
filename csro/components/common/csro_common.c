#include "csro_common.h"
#include "ssl/ssl_crypto.h"


csro_system_info     system_info;
csro_datetime_info   datetime_info;
csro_mqtt_info       mqtt_info;


static void system_get_basic_info(void)
{
    nvs_handle handle;
    nvs_open("system_info", NVS_READWRITE, &handle);
    nvs_get_u32(handle, "power_on_count", &system_info.power_on_count);
    nvs_set_u32(handle, "power_on_count", (system_info.power_on_count + 1));
    nvs_commit(handle);
    nvs_close(handle);

    #ifdef NLIGHT
		sprintf(system_info.device_type, "nlight%d", NLIGHT);
	#elif defined DLIGHT
		sprintf(system_info.device_type, "dlight%d", DLIGHT);
	#elif defined MOTOR
		sprintf(system_info.device_type, "motor%d", MOTOR);
	#elif defined AQI_MONITOR
		sprintf(system_info.device_type, "air_monitor");
	#elif defined AIR_SYSTEM
		sprintf(system_info.device_type, "air_system");
	#endif

    esp_wifi_get_mac(WIFI_MODE_STA, system_info.mac);
    sprintf(system_info.mac_string, "%02x%02x%02x%02x%02x%02x", 
    system_info.mac[0] - 2, system_info.mac[1], system_info.mac[2], system_info.mac[3], system_info.mac[4], system_info.mac[5]);
    sprintf(system_info.host_name, "csro_%s", system_info.mac_string);
}


static void system_get_mqtt_info(void)
{   
    uint8_t sha[30];
	sprintf(mqtt_info.id, "csro/%s", system_info.mac_string);
	sprintf(mqtt_info.name, "csro/%s/%s", system_info.mac_string, system_info.device_type);
	SHA1_CTX *ctx=(SHA1_CTX *)malloc(sizeof(SHA1_CTX));
	SHA1_Init(ctx);
	SHA1_Update(ctx, (const uint8_t *)system_info.mac_string, strlen(system_info.mac_string));
	SHA1_Final(sha, ctx);
	free(ctx);
	sprintf(mqtt_info.pass, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
    sha[0],  sha[2],  sha[4],  sha[6],  sha[8],  sha[1],  sha[3],  sha[5],  sha[7],  sha[9], 
    sha[10], sha[12], sha[14], sha[16], sha[18], sha[11], sha[13], sha[15], sha[17], sha[19]);

    debug("%s, %s, %s\n", mqtt_info.id, mqtt_info.name, mqtt_info.pass);
}


static bool system_get_wifi_info(void)
{
    bool        result = false;
    uint8_t     router_flag = 0;
    nvs_handle  handle;
    
    nvs_open("system_info", NVS_READONLY, &handle);
    nvs_get_u8(handle, "router_flag", &router_flag);
    if(router_flag) {
        size_t ssid_len = 0;
        size_t pass_len = 0;
        nvs_get_str(handle, "ssid", NULL, &ssid_len);
        nvs_get_str(handle, "ssid", system_info.router_ssid, &ssid_len);
        nvs_get_str(handle, "pass", NULL, &pass_len);
        nvs_get_str(handle, "pass", system_info.router_pass, &pass_len);
        result = true;
    }
    nvs_close(handle);
    return result;
}

void csro_system_init(void)
{
    nvs_flash_init();

    system_get_basic_info();
    system_get_mqtt_info();

    csro_datetime_init();

    if (system_get_wifi_info()) {
        xTaskCreate(csro_task_mqtt, "csro_task_mqtt", 3072, NULL, 10, NULL);
    }
    else {
        xTaskCreate(csro_task_smartconfig, "csro_task_smartconfig", 3072, NULL, 10, NULL);
    }
}


void csro_system_set_status(csro_system_status status)
{
    if (system_info.status == RESET_PENDING) {
        return;
    }
    if (system_info.status != status) {
        system_info.status = status;
    }
}