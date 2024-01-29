#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "button-debounce";

#define BUTTON_GPIO (GPIO_NUM_15)
#define LED_GPIO    (GPIO_NUM_18)
#define DEBOUNCE_DELAY_TICK (1)
#define DEBOUNCE_COUNT_FLIP (5)

#define DEBOUNCE_COUNT_MAX (2 * DEBOUNCE_COUNT_FLIP)

enum eButtonState {
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_STATE_INVALID = -1
};

QueueHandle_t xQueue = NULL;

int ping_pong_debounce(bool button_state)
{
    static uint8_t count = DEBOUNCE_COUNT_MAX;

    if (button_state == 0)
    {
        if (count > 0)
        {
            // ESP_LOGI(TAG, "Debounce count: %d", count);
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
            // ESP_LOGI(TAG, "Debounce count: %d", count);
            if (++count == DEBOUNCE_COUNT_FLIP)
            {
                count = DEBOUNCE_COUNT_MAX;
                return BUTTON_RELEASED;
            }
        }
    }

    return BUTTON_STATE_INVALID;
}

void button_task(void *pvParameter)
{

    /*gpio_config */
    gpio_config_t io_config = {0};
    
    io_config.mode = GPIO_MODE_INPUT;
    io_config.pin_bit_mask = 1ULL << BUTTON_GPIO;
    io_config.pull_up_en = GPIO_PULLUP_ENABLE;

    gpio_config(&io_config);

    bool button_state;
    int debounce_state;

    while(1) {

        button_state = gpio_get_level(BUTTON_GPIO);
        debounce_state = ping_pong_debounce(button_state);

        xQueueSend(xQueue, &debounce_state, 5);

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
}

void led_task(void *pvParameter)
{
    /* gpio_config */
    gpio_config_t io_conf = {0};

    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED_GPIO);
    
    gpio_config(&io_conf);

    int debounce_state;

    while(1) {

        if (xQueueReceive(xQueue, &debounce_state, 0) == pdTRUE)
        {
            // ESP_LOGI(TAG, "Debounce state: %d", debounce_state); 
            if (debounce_state == BUTTON_PRESSED)
            {
                // ESP_LOGI(TAG, "Button is pressed");
                gpio_set_level(LED_GPIO, 1);
            }

            if (debounce_state == BUTTON_RELEASED)
            {
                // ESP_LOGI(TAG, "Button is released");
                gpio_set_level(LED_GPIO, 0);
            }
        }
        // vTaskDelay(pdTICKS_TO_MS(10));
    }
    vTaskDelete(NULL);
}
void app_main(void)
{
    xQueue = xQueueCreate(10, sizeof(int));

    xTaskCreate(button_task, "button_task", configMINIMAL_STACK_SIZE+ 2048, NULL, 10, NULL);
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE+ 2048, NULL, 10, NULL);   
}
