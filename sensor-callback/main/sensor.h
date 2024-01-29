// sensor.h
#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>

typedef void (*sensor_alarm_callback_t)(void);

void sensor_init(sensor_alarm_callback_t callback);
void sensor_set_threshold(uint32_t threshold);
void sensor_register_alarm(sensor_alarm_callback_t callback);

#endif // SENSOR_H
