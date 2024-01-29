#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "protocol_examples_common.h"
#include "cJSON.h"
#include "DHT11.h"
#include "driver/gpio.h"
#include "driver/ledc.h"


static const char* TAG = "esp-http-client";

#define DHT11_GPIO (GPIO_NUM_22)
#define TB_ROOT_URI "http://thingsboard.cloud/api/v1/"
#define TB_ACCESS_TOKEN "qkG0ydWa3f2HWKuA0gbF"

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           0
#define LEDC_HS_CH0_GPIO       (GPIO_NUM_18) 
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0
#define LEDC_TEST_DUTY         (4000)
#define LEDC_TEST_FADE_TIME    (3000)

const char* tb_url_telemetry = TB_ROOT_URI TB_ACCESS_TOKEN "/telemetry";
const char* tb_url_shared_attribute = TB_ROOT_URI TB_ACCESS_TOKEN "/attributes?sharedKeys=light_1,light_2,light_3";


#define MAX_BUFFER_SIZE 256

float dht11_temperature = 0.0f;
float dht11_humidity = 0.0f;

uint8_t light_3 = 0;


/* Callback or event handler */
esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;

    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;

    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d, data=%.*s",
            evt->data_len, evt->data_len, (char*)evt->data);

        if (evt->data_len > MAX_BUFFER_SIZE)
            return ESP_FAIL;

        // If user_data buffer is configured, copy the response into the buffer
        if (evt->user_data)
        {
            memcpy(evt->user_data, evt->data, evt->data_len);
        }
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;

    default:
        break;
    }
    return ESP_OK;
}

/* Functions */
esp_err_t http_client_get_req(char* data, const char* url)
{
    esp_err_t ret_code = ESP_FAIL;

    esp_http_client_config_t config = {
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_GET,
        .port = 80,
        .url = url,
        .user_data = data };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    /* This option is only necessary for HTTP_METHOD_POST */
    // esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);

        if (status == 200)
        {
            ESP_LOGI(TAG, "HTTP GET status: %d", status);
            ret_code = ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG, "HTTP GET status: %d", status);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to send GET request");
    }
    esp_http_client_cleanup(client);

    return ret_code;
}

/* Task functions */
void attributes_notify_task(void* arg)
{
    int num_attr_fail = 0;
    while (1)
    {
        char data[MAX_BUFFER_SIZE] = { '\0' };

        /* If the attribute is return correctly */
        if (http_client_get_req(data, tb_url_shared_attribute) == ESP_OK)
        {
            /* Reset num_attr_fail */
            num_attr_fail = 0;

            /* Process JSON to get light_1 and light_2 state */
            cJSON* root = cJSON_Parse(data);
            cJSON* shared = cJSON_GetObjectItem(root, "shared");
            bool light_1 = cJSON_IsTrue(cJSON_GetObjectItem(shared, "light_1"));
            bool light_2 = cJSON_IsTrue(cJSON_GetObjectItem(shared, "light_2"));
            light_3 = (uint)cJSON_GetNumberValue(cJSON_GetObjectItem(shared, "light_3"));

            cJSON_free(root); // Don't forget to free "root"!!!

            // printf("light_1: %d, light_2: %d, light_3: %f\n", light_1, light_2, light_3);
            ESP_LOGI(TAG, "light_1: %d, light_2: %d, light_3: %d\n", light_1, light_2, light_3);
            // update light state here
        }
        /* If the attribute returns fail for 5 times, turn off all the lights (for safety reason) */
        else
        {
            num_attr_fail++;
            if (num_attr_fail >= 10)
            {
                // turn off the lights here
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    vTaskDelete(NULL);
}

void http_post(esp_http_client_handle_t client, float temperature, float humidity)
{
    char post_data[100];
    // Make sure to fill post_data before using it
    sprintf(post_data, "{\"temperature\":%f,\"humidity\":%f}", temperature, humidity);


    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_err_t err = esp_http_client_perform(client);


    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
         esp_http_client_get_status_code(client),
         (long long)esp_http_client_get_content_length(client));

    }
    else {
        ESP_LOGI(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
}

void read_dht11_task(void* arg) {

    DHT11_init(DHT11_GPIO);

    esp_http_client_config_t config = {
        .url = tb_url_telemetry,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 1000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    while (1) {
        dht11_temperature = DHT11_read().temperature;
        dht11_humidity = DHT11_read().humidity;

        ESP_LOGI(TAG, "DHT11 Status: %d", DHT11_read().status);

        http_post(client, dht11_temperature, dht11_humidity);

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    vTaskDelete(NULL);
    esp_http_client_cleanup(client);
}



// LED PWM

void led_init() 
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
        .speed_mode = LEDC_HS_MODE,
        .timer_num = LEDC_HS_TIMER,
    };

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_HS_CH0_CHANNEL,
        .duty = 0,
        .gpio_num = LEDC_HS_CH0_GPIO,
        .intr_type = LEDC_INTR_DISABLE,
        .speed_mode = LEDC_HS_MODE,
        .timer_sel = LEDC_HS_TIMER,
        .hpoint = 0,
    };

    ledc_channel_config(&ledc_channel);

    ledc_fade_func_install(0);

}

void led_set_brightness(uint8_t brightness) 
{
    uint32_t duty = (brightness * 1023) / 100;
    ledc_set_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL, duty);
    ledc_update_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL);
    // ESP_LOGI(TAG, "Duty: %d", duty);
    ESP_LOGI(TAG, "Duty: %" PRIu32, duty);

}

void led_task(void *pvParameters)
{
    led_init();
    while (1)   
    {   
        led_set_brightness(light_3);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
       
}


void app_main(void)
{
    // initialize WiFi
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_err_t err = ESP_FAIL;
    while (err != ESP_OK)
    {
        err = example_connect();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Unable to connect to WiFi.");
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
    
    xTaskCreate(read_dht11_task, "read_dht11_task", configMINIMAL_STACK_SIZE +  4096, NULL, 1, NULL);

    xTaskCreate(attributes_notify_task, "Attributes Update",
        configMINIMAL_STACK_SIZE + 4096, NULL, 2, NULL);
    xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE +  4096, NULL, 1, NULL);
}
