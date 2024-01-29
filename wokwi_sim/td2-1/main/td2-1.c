#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "TP2-2";

#define LED_GPIO (GPIO_NUM_18)
#define BUTTON_GPIO (GPIO_NUM_9)
#define DEBOUNCE_DELAY_TICK (1)
#define DEBOUNCE_COUNT_FLIP (5)

#define DEBOUNCE_COUNT_MAX (2 * DEBOUNCE_COUNT_FLIP)

typedef enum {
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_STATE_INVALID = -1
} eButtonState;

bool button_state;
volatile eButtonState buttonEvent;

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

void debounceTask(void *params)
{

  gpio_config_t io_config = {
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = 1ULL << BUTTON_GPIO,
    .pull_up_en = GPIO_PULLUP_ENABLE
  };

  gpio_config(&io_config);

  while (1)
  {
    button_state = gpio_get_level(BUTTON_GPIO);
    buttonEvent = ping_pong_debounce(button_state);
    if (buttonEvent == BUTTON_PRESSED)
    {
        ESP_LOGI(TAG, "Button is pressed");
    }

    if (buttonEvent == BUTTON_RELEASED)
    {
        ESP_LOGI(TAG, "Button is released");
    }
    vTaskDelay(DEBOUNCE_DELAY_TICK);
  }
}

void blinkTask(void *params)
{
  gpio_config_t io_config = {
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = 1ULL << LED_GPIO
  };

  gpio_config(&io_config);

  while (1)
  {
    switch (buttonEvent)
    {
    case BUTTON_PRESSED:
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        break;

    case BUTTON_RELEASED:
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        break;

    case BUTTON_STATE_INVALID:
    
    default:
        break;
    }
  }

  vTaskDelete(NULL);
}

void app_main(void)
{
  xTaskCreate(debounceTask, "Debounce Task", configMINIMAL_STACK_SIZE + 1024, NULL, 10, NULL);
  xTaskCreate(blinkTask, "Blink Task", configMINIMAL_STACK_SIZE + 1024, NULL, 5, NULL);

  vTaskDelete(NULL);
}