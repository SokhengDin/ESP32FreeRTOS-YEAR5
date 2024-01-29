#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/ledc.h"
#include "driver/adc.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "ADC-SERVO";

#define POTENTIOMETER_ADC_CHANNEL ADC_CHANNEL_6 // ADC1_CHANNEL_0
#define SERVO_GPIO_PIN 18 
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_16_BIT // 16-bit
#define LEDC_FREQUENCY 50 // 50Hz

#define MAX_DUTY 32767 
#define MAX_ADC_VALUE 4095 

int potValue = 0; 

// Function to read potentiometer
void potentioTask(void *arg) {
    // Initialize ADC oneshot mode
    adc_oneshot_unit_handle_t adc1_handle = NULL;
    adc_oneshot_unit_init_cfg_t adc1_init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc1_init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, POTENTIOMETER_ADC_CHANNEL, &channel_config));

    while (1) {
        
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, POTENTIOMETER_ADC_CHANNEL, &potValue));
        // ESP_LOGI(TAG, "Potentiometer value: %d", potValue);
        printf("Potentiometer value: %d\n", potValue);
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
    vTaskDelete(NULL);
}


// Function to control servo
void servoControlTask(void *arg) {
    // Configure LEDC for PWM
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL,
        .duty       = 0,
        .gpio_num   = SERVO_GPIO_PIN,
        .speed_mode = LEDC_MODE,
        .timer_sel  = LEDC_TIMER
    };
    ledc_channel_config(&ledc_channel);

    while (1) {

        uint32_t duty = (potValue * MAX_DUTY) / MAX_ADC_VALUE;
        // Update LEDC duty cycle here
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        // ESP_LOGI(TAG, "Duty: %d", duty);
        // printf("Duty: %ld\n", duty);
        vTaskDelay(pdMS_TO_TICKS(20)); // Delay 20 ms
    }
    vTaskDelete(NULL);
}

void app_main() {
    xTaskCreate(potentioTask, "potentioTask", configMINIMAL_STACK_SIZE+ 2048, NULL, 5, NULL);
    xTaskCreate(servoControlTask, "servoControlTask", configMINIMAL_STACK_SIZE+ 2048, NULL, 5, NULL);
}
