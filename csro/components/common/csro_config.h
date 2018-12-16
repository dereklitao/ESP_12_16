#ifndef CSRO_CONFIG_H_
#define CSRO_CONFIG_H_

#include <stddef.h>
#include <string.h>
#include <time.h>
#include "MQTTClient.h"
#include "MQTTFreeRTOS.h"

#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "FreeRTOS/MQTTFreeRTOS.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#define     DEBUG
#ifdef      DEBUG
   #define  debug(format, ...) printf(format, ## __VA_ARGS__)
#else
   #define  debug(format, ...)
#endif

#define     MQTT_BUFFER_LENGTH      1000
#define     MQTT_TOPIC_LENGTH       100
#define     MQTT_NAME_ID_LENGTH     50

#define     ALARM_NUMBER            20


// #define     NLIGHT                  3
// #define     DLIGHT                  1
// #define     MOTOR                   2
// #define     AIR_MONITOR
#define     AIR_SYSTEM


typedef enum
{
    IDLE                    = 0,
    SMARTCONFIG             = 1,
    SMARTCONFIG_TIMEOUT     = 2,
    NORMAL_START_NOWIFI     = 3,
    NORMAL_START_NOSERVER   = 4,
    NORMAL_START_OK         = 5,
    RESET_PENDING           = 6
} csro_system_status;


typedef struct
{
    char            router_ssid[50];
    char            router_pass[50];

    uint8_t         mac[6];
    uint8_t         ip[4];

    char            host_name[20];
    char            device_type[20];
    char            mac_string[20];
    char            ip_string[20];

    uint32_t        power_on_count;
    uint32_t        wifi_conn_count;
    uint32_t        serv_conn_count;
    uint32_t        publish_count;
    csro_system_status status;
} csro_system_info;


typedef struct 
{
    char            id[MQTT_NAME_ID_LENGTH];
    char            name[MQTT_NAME_ID_LENGTH];
    char            pass[MQTT_NAME_ID_LENGTH];

    char            sub_topic_individual[MQTT_TOPIC_LENGTH];
    char            sub_topic_group[MQTT_TOPIC_LENGTH];
    char            pub_topic[MQTT_TOPIC_LENGTH];

    uint8_t         send_buf[MQTT_BUFFER_LENGTH];
    uint8_t         recv_buf[MQTT_BUFFER_LENGTH];
    uint8_t         content[MQTT_BUFFER_LENGTH];

    char            broker[50];
    char            prefix[50];

    struct Network  network;
    MQTTClient      client;
    MQTTMessage     message;
} csro_mqtt_info;


typedef struct 
{
    time_t          time_on;
    time_t          time_conn;
    time_t          time_now;
    time_t          time_run;
    struct tm       time_info;
    char            strtime[64];
} csro_datetime_info;


typedef struct 
{
    bool            valid;
    uint8_t         weekday;
    uint16_t        minutes;
    uint8_t         action;
} csro_alarm_info;

#endif