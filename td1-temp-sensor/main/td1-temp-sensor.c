#include <stdio.h>
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "ssd1306.h"
#include "DHT.h"

#define SDA_GPIO 5

#define SCL_GPIO 6
#define RESET_GPIO 4
#define SENSOR_GPIO 15

static const char *TAG = "SSD1306";

float tempSensorVal = 0.0f;

typedef struct {
  float temperature;
  float humidity;
} SensorData;

QueueHandle_t sensorQueue;

void temperatureTask(void *params)
{
  DHT11_init(SENSOR_GPIO);
  SensorData data;
  
  while(1)
  {
    data.temperature = DHT11_read().temperature;
    data.humidity = DHT11_read().humidity;
    xQueueSend(sensorQueue, &data, portMAX_DELAY);
    vTaskDelay(500);
  }
  vTaskDelete(NULL);
}

void lcdTask(void *param) {
    SSD1306_t dev;
    ESP_LOGI(TAG, "Initializing I2C interface for SSD1306");
    i2c_master_init(&dev, SDA_GPIO, SCL_GPIO, RESET_GPIO);

    ESP_LOGI(TAG, "SSD1306 initialization");
    ssd1306_init(&dev, 128, 64);

    SensorData data;
    while (1) {
        if(xQueueReceive(sensorQueue, &data, portMAX_DELAY)) {
            char str[32];
            sprintf(str, "Temperature: %f", data.temperature);
            ssd1306_display_text(&dev, 0, str, strlen(str), false);
            sprintf(str, "Humidity: %f", data.humidity);
            ssd1306_display_text(&dev, 1, str, strlen(str), false);
        };

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void app_main(void) {
    sensorQueue = xQueueCreate(10, sizeof(SensorData));
    xTaskCreate(temperatureTask, "temperatureTask", configMINIMAL_STACK_SIZE+ 2048, NULL, 5, NULL);
    xTaskCreate(lcdTask, "lcdTask", configMINIMAL_STACK_SIZE+ 2048, NULL, 5, NULL);
}