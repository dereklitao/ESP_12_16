#ifndef CSRO_COMMON_H_
#define CSRO_COMMON_H_

#include "csro_config.h"

extern csro_system_info     system_info;
extern csro_datetime_info   datetime_info;
extern csro_mqtt_info       mqtt_info;

void csro_system_init(void);
void csro_system_set_status(csro_system_status status);

void csro_task_mqtt(void *pvParameters);
void csro_task_smartconfig(void *pvParameters);

void csro_datetime_init(void);
void csro_datetime_set(char *time_str);

#endif
