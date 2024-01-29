#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "BLINK";

#define LED1 (GPIO_NUM_18)
#define LED2 (GPIO_NUM_21)

void xTaskLed1()
{
    /* gpio_config_t allows configurations many GPIOS at the same time */
    gpio_config_t io_config = {0};
    io_config.mode = GPIO_MODE_INPUT_OUTPUT;
    io_config.pin_bit_mask = 1ULL << LED1;

    /* gpio_config setup the GPIO based on the structure above */
    gpio_config(&io_config);

    while (1)
    {
        bool led_state = gpio_get_level(LED1);
        gpio_set_level(LED1, !led_state);
        // ESP_LOGI(TAG, "LED state = %s", led_state == 0 ? "ON" : "OFF");
        vTaskDelay(200);
    }
}


void xTaskLed2()
{
    /* gpio_config_t allows configurations many GPIOS at the same time */
    gpio_config_t io_config = {0};
    io_config.mode = GPIO_MODE_INPUT_OUTPUT;
    io_config.pin_bit_mask = 1ULL << LED2;

    /* gpio_config setup the GPIO based on the structure above */
    gpio_config(&io_config);

    while (1)
    {
        bool led_state = gpio_get_level(LED2);
        gpio_set_level(LED2, !led_state);
        // ESP_LOGI(TAG, "LED state = %s", led_state == 0 ? "ON" : "OFF");
        vTaskDelay(100);
    }
}


void app_main(void)
{
    xTaskCreate(xTaskLed1, "Blink task 1", 2048, NULL, 2048, NULL);
    xTaskCreate(xTaskLed2, "Blink task 2", 2048, NULL, 2048, NULL);
}
