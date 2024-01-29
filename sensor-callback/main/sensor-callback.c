#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sensor.h"

static const char *TAG = "SENSOR_CALLBACKs";

static void on_sensor_alarm() {
    ESP_LOGI(TAG, "Sensor threshold reached.");
}

void app_main(void) {
    sensor_init(on_sensor_alarm); 
    sensor_set_threshold(2000); 
}
