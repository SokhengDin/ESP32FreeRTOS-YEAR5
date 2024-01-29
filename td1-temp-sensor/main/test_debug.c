#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ssd1306.h"


#define SDA_GPIO 5
#define SCL_GPIO 6
#define RESET_GPIO 4

static const char *TAG = "SSD1306";


void lcdTask(void *param) {
    SSD1306_t dev;
    ESP_LOGI(TAG, "Initializing I2C interface for SSD1306");
    i2c_master_init(&dev, SDA_GPIO, SCL_GPIO, RESET_GPIO);

    ESP_LOGI(TAG, "SSD1306 initialization");
    ssd1306_init(&dev, 128, 64);

  ESP_LOGI(TAG, "'Hello World'");
  ssd1306_display_text(&dev, 0, "Hello World", 11, false);

    while (1) {
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    }


  vTaskDelete(NULL); 
}

void temperatureTask(void *params)
{

}

void app_main(void) {

    xTaskCreate(&lcdTask, "lcdTask", configMINIMAL_STACK_SIZE+2048, NULL, 5, NULL);
}