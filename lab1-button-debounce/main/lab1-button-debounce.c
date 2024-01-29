#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"


static const char *TAG = "button-debounce";

#define BUTTON_GPIO (GPIO_NUM_15)
#define DEBOUNCE_DELAY_TICK (1)
#define DEBOUNCE_COUNT_FLIP (5)
#define BLINK_GPIO (GPIO_NUM_18)

#define DEBOUNCE_COUNT_MAX (2 * DEBOUNCE_COUNT_FLIP)

enum eButtonState {
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_STATE_INVALID = -1
};

bool button_state;


int ping_pong_debounce(bool button_state)
{
    static uint8_t count = DEBOUNCE_COUNT_MAX;

    if (button_state == 0)
    {
        if (count > 0)
        {
            ESP_LOGI(TAG, "Debounce count: %d", count);
            if (--count == DEBOUNCE_COUNT_FLIP)
            {
                count = 0;
                return BUTTON_PRESSED;
            }
        }
    }
    else
    {
        if (count < DEBOUNCE_COUNT_MAX)
        {
            ESP_LOGI(TAG, "Debounce count: %d", count);
            if (++count == DEBOUNCE_COUNT_FLIP)
            {
                count = DEBOUNCE_COUNT_MAX;
                return BUTTON_RELEASED;
            }
        }
    }

    return BUTTON_STATE_INVALID;
}


void debounce_task(void *pvParameter)
{
    /*gpio_config */
    gpio_config_t io_config = {0};
    
    io_config.mode = GPIO_MODE_INPUT;
    io_config.pin_bit_mask = 1ULL << BUTTON_GPIO;
    io_config.pull_up_en = GPIO_PULLUP_ENABLE;

    gpio_config(&io_config);

    int debounce_state;

    while(1) {
        button_state = gpio_get_level(BUTTON_GPIO);
        debounce_state = ping_pong_debounce(button_state);

        if (debounce_state == BUTTON_PRESSED)
        {
            ESP_LOGI(TAG, "Button is pressed");
        }
        if (debounce_state == BUTTON_RELEASED)
        {
            ESP_LOGI(TAG, "Button is released");
        }

        vTaskDelay(DEBOUNCE_DELAY_TICK);
    }
    vTaskDelete(NULL);
}

void blink_task(void *pvParameter)
{
    /*gpio_config */
    gpio_config_t io_config = {0};

    io_config.mode = GPIO_MODE_OUTPUT;
    io_config.pin_bit_mask = 1ULL << BLINK_GPIO;
    gpio_config(&io_config);

    while(1)
    {
        if (button_state == BUTTON_PRESSED)
        {
            gpio_set_level(BLINK_GPIO, 1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "Blinking %s", "ON");

        }
        else if(button_state == BUTTON_RELEASED)
        {
            gpio_set_level(BLINK_GPIO, 0);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "Blinking %s", "OFFidf");

        }  
    }
    vTaskDelete(NULL);
}


void app_main(void)
{
    xTaskCreate(&debounce_task, "debounce_task", 2048, NULL, 5, NULL);
    xTaskCreate(&blink_task, "blink_task", 2048, NULL, 5, NULL);
}
