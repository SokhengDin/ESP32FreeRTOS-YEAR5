#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi.h"

static wifi_connected_callback_t on_wifi_connected = NULL;

/*-----------------------------Tasks Definitions-----------------------------*/
static void wifiTask(void *args)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        on_wifi_connected();
    }
    vTaskDelete(NULL);
}

/*-----------------------------Function Definitions-----------------------------*/
void wifi_register_callback(wifi_connected_callback_t callback)
{
    if (callback != NULL)
    {
        on_wifi_connected = callback;
    }
}

void wifi_start(void)
{
    xTaskCreate(wifiTask, "Wifi Task", configMINIMAL_STACK_SIZE + 1024, NULL, 1, NULL);
}