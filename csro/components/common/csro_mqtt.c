#include "csro_common.h"
#include "cJSON.h"
#include "../device/csro_device.h"


static EventGroupHandle_t   wifi_event_group;
static SemaphoreHandle_t    basic_msg_semaphore;
static SemaphoreHandle_t    system_msg_semaphore;
static SemaphoreHandle_t    timer_msg_semaphore;
static TimerHandle_t        basic_msg_timer = NULL;
static const EventBits_t    CONNECTED_BIT   = BIT0;
int                         udp_sock;



static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    if (event->event_id == SYSTEM_EVENT_STA_START) {
        tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, system_info.host_name);
        esp_wifi_connect();
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


static bool create_udp_server(void)
{
    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_sock < 0) {
        return false;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(udp_sock);
        return false;
    }
    return true;
}


static void udp_receive_task(void *pvParameters)
{
    static char udp_rx_buffer[512];
    while(true)
    {
        bool sock_status = false;
        while(sock_status == false)
        {
            vTaskDelay(1000 / portTICK_RATE_MS);
            sock_status = create_udp_server();
        }
        while(true)
        {
            struct sockaddr_in source_addr;
            socklen_t socklen = sizeof(source_addr);
            bzero(udp_rx_buffer, 512);
            int len = recvfrom(udp_sock, udp_rx_buffer, sizeof(udp_rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            if (len < 0) {
                break;
            }
            cJSON *serv_ip, *serv_mac, *time_info;
            cJSON *json = cJSON_Parse(udp_rx_buffer);
            if (json != NULL) {
                serv_ip = cJSON_GetObjectItem(json, "server");
                serv_mac = cJSON_GetObjectItem(json, "clientid");
                time_info = cJSON_GetObjectItem(json, "time");
                if ((serv_ip != NULL) && (serv_mac != NULL) && (serv_ip->type == cJSON_String) && (serv_mac->type == cJSON_String)) {
                    if (strlen(serv_ip->valuestring)>=4 && strlen(serv_mac->valuestring)>=10) {
                        if ((strcmp((char *)serv_ip->valuestring, (char *)mqtt_info.broker) != 0) || (strcmp((char *)serv_mac->valuestring, (char *)mqtt_info.prefix) != 0)) {
                            strcpy((char *)mqtt_info.broker, (char *)serv_ip->valuestring);
                            strcpy((char *)mqtt_info.prefix, (char *)serv_mac->valuestring);
                            mqtt_info.client.isconnected = 0;
                        }
                    }
                }
                if ((time_info != NULL) && (time_info->type == cJSON_String)) {
                    csro_datetime_set(time_info->valuestring);
                }
            }
            cJSON_Delete(json);
        }
    }
    vTaskDelete(NULL);
}





static void connect_wifi(void)
{

    csro_system_set_status(NORMAL_START_NOWIFI);
    wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    esp_event_loop_init(wifi_event_handler, NULL);

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&config);
    wifi_config_t wifi_config;
    strcpy((char *)wifi_config.sta.ssid, (char *)system_info.router_ssid);
    strcpy((char *)wifi_config.sta.password, (char *)system_info.router_pass);

    debug("%s, %s\r\n", wifi_config.sta.ssid, wifi_config.sta.password);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    xTaskCreate(udp_receive_task, "udp_receive_task", 2048, NULL, 5, NULL);
} 


static bool wifi_is_connected(void)
{
    static bool connect = false;
    tcpip_adapter_ip_info_t info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &info);
    if (info.ip.addr != 0) {
        if (connect == false) {
            connect = true;
            csro_system_set_status(NORMAL_START_NOSERVER);
            system_info.ip[0] = info.ip.addr&0xFF;
            system_info.ip[1] = (info.ip.addr&0xFF00)>>8;
            system_info.ip[2] = (info.ip.addr&0xFF0000)>>16;
            system_info.ip[3] = (info.ip.addr&0xFF000000)>>24;
            sprintf(system_info.ip_string, "%d.%d.%d.%d", system_info.ip[0], system_info.ip[1], system_info.ip[2], system_info.ip[3]);
            debug("IP = %s\r\n", system_info.ip_string);
        } 
    }
    else {
        csro_system_set_status(NORMAL_START_NOWIFI);
        connect = false;
    }
    return connect;
}

static void handle_individual_message(MessageData* data)
{
    csro_device_handle_individual_message(data);
}

static void handle_group_message(MessageData* data)
{
    csro_device_handle_group_message(data);
}




static bool broker_is_connected(void)
{
    if (mqtt_info.client.isconnected == 1) {
        return true;
    }
    if (strlen((char *)mqtt_info.broker) == 0 || strlen((char *)mqtt_info.prefix) == 0) {
        return false;
    }
    close(mqtt_info.network.my_socket);
    NetworkInit(&mqtt_info.network);
    debug("id = %s, name = %s, password = %s\r\n", mqtt_info.id, mqtt_info.name, mqtt_info.pass);
    if (NetworkConnect(&mqtt_info.network, mqtt_info.broker, 1883) != SUCCESS) {
        return false;
    }
    MQTTClientInit(&mqtt_info.client, &mqtt_info.network, 5000, mqtt_info.send_buf, MQTT_BUFFER_LENGTH, mqtt_info.recv_buf, MQTT_BUFFER_LENGTH);
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.clientID.cstring = mqtt_info.id;
    data.username.cstring = mqtt_info.name;
    data.password.cstring = mqtt_info.pass;
    if (MQTTConnect(&mqtt_info.client, &data) != SUCCESS) {
        return false;
    }
    sprintf(mqtt_info.sub_topic_individual, "%s/%s/%s/command", mqtt_info.prefix, system_info.mac_string, system_info.device_type);
    sprintf(mqtt_info.sub_topic_group, "%s/group", mqtt_info.prefix);

    if (MQTTSubscribe(&mqtt_info.client, mqtt_info.sub_topic_individual, QOS1, handle_individual_message) != SUCCESS) {
        mqtt_info.client.isconnected = 0;
        return false;
    }
    debug("sub ok with topic: %s\r\n", mqtt_info.sub_topic_individual);

    if (MQTTSubscribe(&mqtt_info.client, mqtt_info.sub_topic_group, QOS1, handle_group_message) != SUCCESS) {
        mqtt_info.client.isconnected = 0;
        return false;
    }
    debug("sub ok with topic: %s\r\n", mqtt_info.sub_topic_group);
    return true;
}

static void basic_msg_timer_callback( TimerHandle_t xTimer )
{
    xSemaphoreGive(basic_msg_semaphore);
}

void trigger_basic_message(void)
{
    xTimerReset(basic_msg_timer, 0);
    xSemaphoreGive(basic_msg_semaphore);
}

void trigger_system_message(void)
{
    xSemaphoreGive(system_msg_semaphore);
}

void trigger_timer_message(void)
{
    xSemaphoreGive(timer_msg_semaphore);
}

static bool mqtt_pub_basic_message(void)
{
    csro_device_prepare_basic_message();
	sprintf(mqtt_info.pub_topic, "%s/%s/%s/basic", mqtt_info.prefix, system_info.mac_string, system_info.device_type);
	mqtt_info.message.payload = mqtt_info.content;
	mqtt_info.message.payloadlen = strlen(mqtt_info.message.payload);
	mqtt_info.message.qos = QOS1;
	if (MQTTPublish(&mqtt_info.client, mqtt_info.pub_topic, &mqtt_info.message) != SUCCESS) {
        mqtt_info.client.isconnected = 0;
        return false;
    }
	return true;
}

static bool mqtt_pub_system_message(void)
{
    return true;
}

static bool mqtt_pub_timer_message(void)
{
    return true;
}


void csro_task_mqtt(void *pvParameters)
{
    connect_wifi();
    vSemaphoreCreateBinary(basic_msg_semaphore);
    vSemaphoreCreateBinary(system_msg_semaphore);
    vSemaphoreCreateBinary(timer_msg_semaphore);
    basic_msg_timer = xTimerCreate("basic_msg_timer", 3000/portTICK_RATE_MS, pdTRUE, (void *)0, basic_msg_timer_callback);
    xTimerStart(basic_msg_timer, 0);

    while(true)
    {
        if (wifi_is_connected()) {
            if (broker_is_connected()) {
                csro_system_set_status(NORMAL_START_OK);
            //     if (xSemaphoreTake(basic_msg_semaphore, 0) == pdTRUE) {
            //        mqtt_pub_basic_message();
            //    }
                 mqtt_pub_basic_message();
                if (xSemaphoreTake(system_msg_semaphore, 0) == pdTRUE) {
                   mqtt_pub_system_message();
               }
                if (xSemaphoreTake(timer_msg_semaphore, 0) == pdTRUE) {
                   mqtt_pub_timer_message();
               }
                if (MQTTYield(&mqtt_info.client, 1) != SUCCESS) {
                   mqtt_info.client.isconnected = 0;
               }
            }
            else {
                csro_system_set_status(NORMAL_START_NOSERVER);
                vTaskDelay(1000 / portTICK_RATE_MS);
            }
        }
        else {
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    }
    vTaskDelete(NULL);
}
