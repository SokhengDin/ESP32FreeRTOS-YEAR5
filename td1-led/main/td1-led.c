#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "BLINK_BLINK";

#define PIN_LED_1 (GPIO_NUM_13)
#define PIN_LED_2 (GPIO_NUM_19)


void task_led1()
{
    gpio_config_t io_config = {0};
    io_config.mode = GPIO_MODE_OUTPUT;
    io_config.pin_bit_mask = 1ULL << PIN_LED_1;

    gpio_config(&io_config);

    while(1)
    {
        bool led1_state = gpio_get_level(PIN_LED_1);
        gpio_set_level(PIN_LED_1, !led1_state);
        ESP_LOGI(TAG, "LED1 Blink status = %s", led1_state == 0 ? "ON" : "OFF");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void task_led2()
{
    gpio_config_t io_config = {0};
    io_config.mode = GPIO_MODE_OUTPUT;
    io_config.pin_bit_mask = 1ULL << PIN_LED_2;

    gpio_config(&io_config);

    while(1)
    {
        bool led2_state = gpio_get_level(PIN_LED_2);
        gpio_set_level(PIN_LED_2, !led2_state);
        ESP_LOGI(TAG, "LED2 Blink status = %s", led2_state == 0 ? "ON" : "OFF");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    xTaskCreate(task_led1, "BLINK_LED1", 2048, NULL, 5, NULL);
    xTaskCreate(task_led2, "BLINK_LED2", 2048, NULL, 5, NULL);
}
