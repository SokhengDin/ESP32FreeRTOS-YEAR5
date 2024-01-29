#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "blinky_led";

#define BUILTIN_LED (GPIO_NUM_2)


void app_main(void)
{
    /* gpio_config_t allows configuring many GPIOS at the same time */
    gpio_config_t io_config = {0};
    io_config.mode = GPIO_MODE_OUTPUT;
    io_config.pin_bit_mask = 1ULL << BUILTIN_LED;

    /* gpio_config setup the GPIO based on the struture above */
    gpio_config(&io_config);

    while (1)
    {
        bool led_state = gpio_get_level(BUILTIN_LED); // get current LED state
        gpio_set_level(BUILTIN_LED, !led_state);      // set new LED state
        ESP_LOGI(TAG, "LED state = %s", led_state == 0 ? "ON" : "OFF");
        vTaskDelay(40); // delay for 40 ticks = 400ms   
    }
    
}
