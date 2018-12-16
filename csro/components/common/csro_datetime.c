#include "csro_common.h"

TimerHandle_t       second_timer = NULL;
bool                time_sync = false;
csro_alarm_info     alarm_group[ALARM_NUMBER];










static void datetime_check_alarm(void)
{
    uint16_t now_minutes = datetime_info.time_info.tm_hour * 60 + datetime_info.time_info.tm_min;
    uint8_t now_weekday = datetime_info.time_info.tm_wday;
    for(size_t i = 0; i < ALARM_NUMBER; i++) 
    {
        if (alarm_group[i].valid == true && alarm_group[i].minutes == now_minutes) {
            if ((alarm_group[i].weekday & (0x01<<now_weekday)) == (0x01<<now_weekday)) {
                debug("alarm on @ %d, %d, %d\r\n", now_weekday, now_minutes, alarm_group[i].action);
            }
        }
    }
}


static void datetime_second_timer_callback( TimerHandle_t xTimer )
{
    datetime_info.time_run++;
    if (time_sync == true) {
        datetime_info.time_now++;
        localtime_r(&datetime_info.time_now, &datetime_info.time_info);
        if (datetime_info.time_info.tm_sec == 0) {
            datetime_check_alarm();
        }
    }
}


void csro_datetime_init(void)
{
    second_timer = xTimerCreate("second_timer", 1000/portTICK_RATE_MS, pdTRUE, (void *)0, datetime_second_timer_callback);
    if (second_timer != NULL) {
        xTimerStart(second_timer, 0);
    }
}


void csro_datetime_set(char *time_str)
{
    uint32_t tim[6];
    sscanf(time_str, "%d-%d-%d %d:%d:%d", &tim[0], &tim[1], &tim[2], &tim[3], &tim[4], &tim[5]);
    if ((tim[0]<2018) || (tim[1]>12) || (tim[2]>31) || (tim[3]>23) || (tim[4]>59) || (tim[5]>59)) {
        return;
    }
    if (time_sync == false || tim[5] == 30) {
        datetime_info.time_info.tm_year     = ((tim[0] % 2000) + 2000) - 1900;
        datetime_info.time_info.tm_mon      = tim[1] - 1;
        datetime_info.time_info.tm_mday     = tim[2];
        datetime_info.time_info.tm_hour     = tim[3];
        datetime_info.time_info.tm_min      = tim[4];
        datetime_info.time_info.tm_sec      = tim[5];
        datetime_info.time_now              = mktime(&datetime_info.time_info);

        if (time_sync == false) {
            time_sync = true;
            datetime_info.time_on = datetime_info.time_now - datetime_info.time_run;
        }
    }
}