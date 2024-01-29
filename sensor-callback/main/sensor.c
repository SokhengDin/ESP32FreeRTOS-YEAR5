// sensor.c
#include "sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/adc.h"

static sensor_alarm_callback_t alarm_callback = NULL;
static uint32_t threshold_millivolts;

static void sensor_task(void *pvParameters) {
    while (1) {
        uint32_t adc_reading = adc1_get_raw(ADC1_CHANNEL_0); 
        uint32_t voltage = (adc_reading * 3300) / 4095; 

        if (voltage >= threshold_millivolts && alarm_callback != NULL) {
            alarm_callback();
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

void sensor_init(sensor_alarm_callback_t callback) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
    xTaskCreate(sensor_task, "sensorTask", configMINIMAL_STACK_SIZE+2048, NULL, 5, NULL);
    sensor_register_alarm(callback);
}

void sensor_set_threshold(uint32_t threshold) {
    threshold_millivolts = threshold;
}

void sensor_register_alarm(sensor_alarm_callback_t callback) {
    alarm_callback = callback;
}
