#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"


static const char *TAG = "blinky-led";

#define LED_GPIO (GPIO_NUM_18)

void app_main(void)
{
    /* gpio_config_t allows configuring many GPIOs at the same time */
    gpio_config_t io_config = {0};
    io_config.mode = GPIO_MODE_OUTPUT;
    io_config.pin_bit_mask = (1ULL << LED_GPIO);
    io_config.pull_down_en = 0;
    io_config.pull_up_en = 0;

    /* gpio_config setup the GPIO based on the structure above */
    gpio_config(&io_config);

    while (1)
    {
        bool led_state = gpio_get_level(LED_GPIO);
        gpio_set_level(LED_GPIO, !led_state);
        ESP_LOGI(TAG, "Led state = %s", led_state == 0 ? "ON" : "OFF");
        vTaskDelay(40);
    }

}