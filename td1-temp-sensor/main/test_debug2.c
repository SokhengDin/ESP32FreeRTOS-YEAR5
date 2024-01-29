#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "DHT.h"
#include "esp_system.h"
#include "esp_log.h"
#include "ssd1306.h"

TaskHandle_t Task1 = NULL;
TaskHandle_t Task2 = NULL;
QueueHandle_t queue;

#define SDA_GPIO 5
#define SCL_GPIO 6
#define RESET_GPIO 4
#define SENSOR_GPIO 15

static const char *TAG = "SSD1306";


SSD1306_t dev;

void Get_value_from_sensor(void* TeamBoyLoy){

  float Get_value_from_sensor;
  for(;;){

        printf("Temperature is %d \n", DHT11_read().temperature);
        printf("Humidity is %d\n", DHT11_read().humidity);
        vTaskDelay(500);
    queue = xQueueCreate(10,sizeof(Get_value_from_sensor));
    if (queue == 0){

      printf ("No Data from sensor %p \n",queue);
    }

    xQueueSend(queue,(void*)&Get_value_from_sensor,(TickType_t)5);

      while(1){
          vTaskDelay(1000/ portTICK_PERIOD_MS);
      }
  }
}

void Display_data(void *TeamBoyLoy) {
  float Get_Data_from_sensor = 0.0f;
  char data[50];

  ESP_LOGI(TAG, "Initializing I2C interface for SSD1306");
  i2c_master_init(&dev, SDA_GPIO, SCL_GPIO, RESET_GPIO);

  ESP_LOGI(TAG, "SSD1306 initialization");
  ssd1306_init(&dev, 128, 64);
  
  snprintf(data, sizeof(data), "%f", Get_Data_from_sensor);
  while(1) {

    if (xQueueReceive(queue, (void*) &Get_Data_from_sensor,(TickType_t) 5)) {
     ssd1306_display_text(&dev, 0, data, 50, false);

      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }

  vTaskDelete(NULL);
}

void app_main(void)
{

    while(1){
      xTaskCreate(&Get_value_from_sensor, "Sensor", configMINIMAL_STACK_SIZE+4096, NULL, 5, NULL);
      xTaskCreate(&Display_data, "Display_data", configMINIMAL_STACK_SIZE+4096, NULL, 5, NULL);
    }
}
