#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"


extern const uint8_t mqtt_ca_certificate_pem_start[]   asm("_binary_mqtt_ca_certificate_pem_start");
extern const uint8_t mqtt_ca_certificate_pem_end[]   asm("_binary_mqtt_ca_certificate_pem_end");


// WIFI SETUP
#define WIFI_SSID "Lab-H202"
#define WIFI_PASS "h202-eRoxi@23"
#define MAX_RETRY  5

// MQTT SETUP
#define MQTT_BROKER_URI "wss://dd0842fcc8b945aea3193087b930ba01.s1.eu.hivemq.cloud:8884/mqtt"
#define MQTT_PORT 8884
#define MQTT_USER "SokhengDin"
#define MQTT_PASS "Rosdev@23@ai"

// QUEUE SETUP
#define MAX_QUEUE_SIZE 10

// MQTT TOPICS

#define MQTT_PUBLISH "esp32/voice/publish"
#define MQTT_SUBSCRIBE "esp32/voice/subscribe"

static const char *TAG = "MQTT_VOICE";
static EventGroupHandle_t wifi_event_group;
static int retry_num = 0;

static esp_mqtt_client_handle_t mqtt_client = NULL;
static QueueHandle_t mqtt_queue = NULL;

const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;
static int count = 0;


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Connecting to %s...", WIFI_SSID);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < MAX_RETRY) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "Retrying to connect to %s... (%d/%d)", WIFI_SSID, retry_num, MAX_RETRY);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG, "Failed to connect to %s after %d retries", WIFI_SSID, MAX_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_task(void *pvParameters) {
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP with SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to AP with SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(wifi_event_group);
    vTaskDelete(NULL);
}



static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id ;

    switch (event->event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, MQTT_SUBSCRIBE, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_DATA:
            if (strncmp(event->topic, MQTT_SUBSCRIBE, event->topic_len) == 0) {
                char* received_msg = strndup(event->data, event->data_len);
                if (received_msg) {
                    ESP_LOGI(TAG, "Received on topic %s: %s", MQTT_SUBSCRIBE, received_msg);
                    free(received_msg);
                }
            }
            break;

        default:
            break;
    }
}


// MQTT Task

static void mqtt_task(void *pvParameter)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .broker.address.port = MQTT_PORT,
        .broker.verification.certificate = (const char *)mqtt_ca_certificate_pem_start,
        .credentials.username = MQTT_USER,
        .credentials.authentication.password = MQTT_PASS,
    };

    esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    char *message;
    
    while (1) {
        if (xQueueReceive(mqtt_queue, &message, portMAX_DELAY)) {
            esp_mqtt_client_publish(mqtt_client, MQTT_PUBLISH, message, strlen(message), 1, 0);
            ESP_LOGI(TAG, "Message published: %s", message);
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to avoid busy looping
    }
    vTaskDelete(NULL);
}


// Function to Send Message to MQTT
void send_hello_world_task(void *pvParameter) {
    
    char message[50];



    while (1) {
        sprintf(message, "Hello world, %d", count);

        char *message_copy = strdup(message);
        if (xQueueSend(mqtt_queue, &message_copy, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Failed to enqueue message");
        }

        count ++;
        vTaskDelay(pdMS_TO_TICKS(1000)); // Send message every 5 seconds
    }
}

void app_main() {

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    xTaskCreate(&wifi_task, "wifi_task", configMINIMAL_STACK_SIZE+4096, NULL, 5, NULL);

    // Create MQTT queue
    mqtt_queue = xQueueCreate(MAX_QUEUE_SIZE, sizeof(char *));
    if (mqtt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT queue");
        return;
    }

    // Start MQTT task
    xTaskCreate(mqtt_task, "mqtt_task", configMINIMAL_STACK_SIZE+4096, NULL, 5, NULL);

    // Example usage of send_mqtt_message
    xTaskCreate(send_hello_world_task, "send_hello_world_task",configMINIMAL_STACK_SIZE+ 4096, NULL, 5, NULL);
}