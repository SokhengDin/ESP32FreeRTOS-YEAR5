#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi.h"

static const char *TAG = "LAB2_CALLBACK";

static void on_wifi_connected()
{
    ESP_LOGI(TAG, "Wifi is connected");
}

void app_main(void)
{
    wifi_register_callback(on_wifi_connected);
    wifi_start();

    vTaskDelete(NULL);
}