#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "driver/gpio.h"


#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include "DHT11.h"

#define PRO_CORE 0
#define APP_CORE 1

#define WIFI_TASK_PRIORITY 10
#define SENSOR_TASK_PRIORITY 5
#define MQTT_TASK_PRIORITY 8
#define HEALTH_TASK_PRIORITY 3

#define MAXIMUM_RETRY  5
#define SSID           "Lab-H202"
#define PASSWORD       "h202-eRoxi@23"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define CONFIG_BROKER_BIN_SIZE_TO_SEND 0x10000
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define Debounce_count_flip (4)
#define Debounce_count_max (2* Debounce_count_flip)
#define DHT22_GPIO 27

int Relay_pins[] = {19,21,22,23};       // Relay Pin Control
int Touch_Pins[] = {12,13,14,32};   // Touch sensor Pin
int Relay_State[] = {0,0,0,0};
int Last_State[] = {0,0,0,0};
int Current_State[] = {0,0,0,0};

float dht11_temp = 0.0f;
float dht11_hudm = 0.0f;

enum TouchState
{
    Touch_Pressed,
    Touch_Release,
    Touch_Invalid = -1
};

static const char *TAG = "ESP32-SmartHome";
static const char *SYSTEM_HEALTH_TAG = "SYSTEM_HEALTH";
static int s_retry_num = 0;
static EventGroupHandle_t wifi_event_group;

SemaphoreHandle_t xMutex = NULL;

// Ping Pong Algorithm
int ping_pong_debounce(bool Touch_State ,uint8_t *count){
    if(Touch_State == 1){
        if(*count > 0){
            if(--(*count) == Debounce_count_flip){
                *count = 0;
                return Touch_Pressed;
            }
        }
    }
    else{
        if(*count < Debounce_count_max){
            if(++(*count) == Debounce_count_flip){
                *count = Debounce_count_max;
                return Touch_Release;
            }
        }
    }
    return Touch_Invalid;
}

void Touch_Sensor(void *arg){

    bool Touch1,Touch2,Touch3,Touch4;
    int T_Debounce1,T_Debounce2,T_Debounce3,T_Debounce4;

    // Queue for Relay Control
    int msg[2];

    // Touch Sensor Configuration Pins
    gpio_config_t Sensor_Config = {0};
    Sensor_Config.mode = GPIO_MODE_INPUT;
    Sensor_Config.pull_up_en = GPIO_PULLUP_ENABLE;
    Sensor_Config.pin_bit_mask = (1ULL << Touch_Pins[0]) | (1ULL << Touch_Pins[1]) | (1ULL << Touch_Pins[2]) | (1ULL << Touch_Pins[3]) ;
    gpio_config(&Sensor_Config);

    // Relay Configuration Pins
    gpio_config_t Relay_Config = {0};
    Relay_Config.mode = GPIO_MODE_OUTPUT;
    Relay_Config.pin_bit_mask = (1ULL << Relay_pins[0]) | (1ULL << Relay_pins[1]) | (1ULL << Relay_pins[2]) | (1ULL << Relay_pins[3]) ;
    gpio_config(&Relay_Config);

    uint8_t count1,count2,count3,count4 = Debounce_count_max;
    
    while (1)
    {

        //Touch Sensor Control Relay 1
        Touch1 = gpio_get_level(Touch_Pins[0]);
        T_Debounce1 = ping_pong_debounce(Touch1,&count1);
        Last_State[0] = Current_State[0];
        Current_State[0] = Touch1;
        if(T_Debounce1 == Touch_Pressed){
            ESP_LOGI(TAG,"Sensor Touch 1 Pressed");
        }
        if(T_Debounce1 == Touch_Release){
            ESP_LOGI(TAG,"Sensor Touch 1 Released");
        }
        

        //Touch Sensor Control Relay 2
        Touch2 = gpio_get_level(Touch_Pins[1]);
        T_Debounce2 = ping_pong_debounce(Touch2,&count2);
        Last_State[1] = Current_State[1];
        Current_State[1] = Touch2;
        if(T_Debounce2 == Touch_Pressed){
            ESP_LOGI(TAG,"Sensor Touch 2 Pressed");
        }
        if(T_Debounce2 == Touch_Release){
            ESP_LOGI(TAG,"Sensor Touch 2 Released");
        }

        //Touch Sensor Control Relay 3
        Touch3 = gpio_get_level(Touch_Pins[2]);
        T_Debounce3 = ping_pong_debounce(Touch3,&count3);
        Last_State[2] = Current_State[2];
        Current_State[2] = Touch3;
        if(T_Debounce3 == Touch_Pressed){
            ESP_LOGI(TAG,"Sensor Touch 3 Pressed");
        }
        if(T_Debounce3 == Touch_Release){
            ESP_LOGI(TAG,"Sensor Touch 3 Released");
        }

        //Touch Sensor Control Relay 4
        Touch4 = gpio_get_level(Touch_Pins[3]);
        T_Debounce4 = ping_pong_debounce(Touch4,&count4);
        Last_State[3] = Current_State[3];
        Current_State[3] = Touch4;
        if(T_Debounce4 == Touch_Pressed){
            ESP_LOGI(TAG,"Sensor Touch 4 Pressed");
        }
        if(T_Debounce4 == Touch_Release){
            ESP_LOGI(TAG,"Sensor Touch 4 Released");
        }

        if (xSemaphoreTake(xMutex, (TickType_t)10) == pdTRUE)
        {

            if(Last_State[0] == Touch_Release && Current_State[0] == Touch_Pressed){
                if(Relay_State[0] == 0){
                    Relay_State[0] = 1;
                    ESP_LOGI(TAG,"Relay 1 ON");
                }else{Relay_State[0] = 0;
                    ESP_LOGI(TAG,"Relay 1 OFF");}
                gpio_set_level(Relay_pins[0], Relay_State[0]);
            }

            if(Last_State[1] == Touch_Release && Current_State[1] == Touch_Pressed){
                if(Relay_State[1] == 0){
                    Relay_State[1] = 1;
                    ESP_LOGI(TAG,"Relay 2 ON");
                }else{Relay_State[1] = 0;
                    ESP_LOGI(TAG,"Relay 2 OFF");}
                gpio_set_level(Relay_pins[1],Relay_State[1]);
            }

            if(Last_State[2] == Touch_Release && Current_State[2] == Touch_Pressed){
                if(Relay_State[2] == 0){
                    Relay_State[2] = 1;
                    ESP_LOGI(TAG,"Relay 3 ON");
                }else{Relay_State[2] = 0;
                    ESP_LOGI(TAG,"Relay 3 OFF");}
                gpio_set_level(Relay_pins[2],Relay_State[2]);
            }


            if(Last_State[3] == Touch_Release && Current_State[3] == Touch_Pressed){
                if(Relay_State[3] == 0){
                Relay_State[3] = 1;
                ESP_LOGI(TAG,"Relay 4 ON");
                }else{Relay_State[3] = 0;
                ESP_LOGI(TAG,"Relay 4 OFF");}
                gpio_set_level(Relay_pins[3],Relay_State[3]);
            }

            xSemaphoreGive(xMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}



void DHT_task(void *pvParameter)
{
	// setDHTgpio( DHT22_GPIO );
    DHT11_init(DHT22_GPIO); 
	// printf( "Starting DHT Task\n\n");

	while(1) {
	
		// printf("=== Reading DHT ===\n" );
		// int ret = readDHT();
		
        // errorHandler(ret);

        // dht22_hudm = getHumidity();
        dht11_hudm = DHT11_read().humidity;
		// printf( "Hum %.1f\n", getHumidity() );
        // printf( "Hum %.1f\n", dht11_hudm);

        // dht22_temp = getTemperature();
        dht11_temp = DHT11_read().temperature;
		// printf( "Tmp %.1f\n", getTemperature() );
        // printf( "Tmp %.1f\n", dht11_temp);
		
        ESP_LOGI(TAG, "DHT11 Status: %d", DHT11_read().status);

		// -- wait at least 2 sec before reading again ------------
		// The interval of whole process must be beyond 2 seconds !! 
		vTaskDelay( pdMS_TO_TICKS(3000) );
	}
    vTaskDelete(NULL);
}

// MQTT event publisher for DHT22 sensor data
static void publish_dht22_data(esp_mqtt_client_handle_t client, const char* temperature_topic, const char* humidity_topic, float temperature, float humidity) {
    char temp_str[32];
    char hum_str[32];

    // Convert float to string
    snprintf(temp_str, sizeof(temp_str), "%f", temperature);
    snprintf(hum_str, sizeof(hum_str), "%f", humidity);

    // Publish temperature
    int msg_id_temp = esp_mqtt_client_publish(client, temperature_topic, temp_str, 0, 1, 0);
    ESP_LOGI(TAG, "Temperature published with msg_id=%d", msg_id_temp);

    // Publish humidity
    int msg_id_hum = esp_mqtt_client_publish(client, humidity_topic, hum_str, 0, 1, 0);
    ESP_LOGI(TAG, "Humidity published with msg_id=%d", msg_id_hum);
}


// Wi-Fi event handler
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialize the wifi
void wifi_init_sta(void *pvParameter)
{
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
            .ssid = SSID,
            .password = PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    while (1)
    {
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to AP SSID:%s",
                    SSID);
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                    SSID);
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
    vTaskDelete(NULL);

}



// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    
        char switch_topics[4][30] = {
            "esp_mqtt/switch/switch_1",
            "esp_mqtt/switch/switch_2",
            "esp_mqtt/switch/switch_3",
            "esp_mqtt/switch/switch_4"
        };

        int msg_ids[4];

        for (int i = 0; i < 4; i++) {
            msg_ids[i] = esp_mqtt_client_subscribe(client, switch_topics[i], 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_ids[i]);
        }
        

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        esp_mqtt_client_reconnect(client);
        break;


    case MQTT_EVENT_SUBSCRIBED:

        break;


    case MQTT_EVENT_PUBLISHED:
    
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);

        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        for (int i = 0; i < 4; i++) {
            char topic[50];
            snprintf(topic, sizeof(topic), "esp_mqtt/switch/switch_%d", i + 1);
            if (strncmp(event->topic, topic, event->topic_len) == 0) {
                int newState = strncmp(event->data, "ON", event->data_len) == 0 ? 1 : 0;
                const char *payload = newState ? "ON" : "OFF";
                if (Relay_State[i] != newState) {
                    Relay_State[i] = newState;
                    gpio_set_level(Relay_pins[i], Relay_State[i]);
                    printf("Relay %d state changed to: %d\n", i + 1, Relay_State[i]);
                    esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
                }
            }
        }
        break;



    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void *pvParmeter)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = "mqtt://192.168.1.102",
            .address.port = 1883,
        },
        .credentials = {
            .username = "esp_mqtt",
            .authentication.password = "rosdev@23@ai",
        }
    };

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            if (client != NULL && !esp_mqtt_client_disconnect(client)) {
                publish_dht22_data(client, "esp_mqtt/temperature", "esp_mqtt/humidity", dht11_temp, dht11_hudm);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    vTaskDelete(NULL);
}



void system_health_task(void *pvParameter) {
    while (1) {
        ESP_LOGI(SYSTEM_HEALTH_TAG, "Free heap size: %" PRIu32 " bytes", esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000)); // Every 10 seconds
    }
    vTaskDelete(NULL);
}


void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    xMutex = xSemaphoreCreateMutex();


    if (xMutex == NULL)
    {
        ESP_LOGE(TAG, "Mutex creation failed");
    }
    else {
        ESP_LOGI(TAG, "Mutex creation successful");

        // Create tasks with appropriate priorities
        xTaskCreatePinnedToCore(wifi_init_sta, "wifi_init_sta", configMINIMAL_STACK_SIZE + 4096, NULL, WIFI_TASK_PRIORITY, NULL, PRO_CORE);
        xTaskCreatePinnedToCore(Touch_Sensor, "Touch_Sensor", configMINIMAL_STACK_SIZE + 2048, NULL, SENSOR_TASK_PRIORITY, NULL, APP_CORE);
        xTaskCreatePinnedToCore(DHT_task, "DHT_task", configMINIMAL_STACK_SIZE + 2048, NULL, SENSOR_TASK_PRIORITY, NULL, APP_CORE);
        xTaskCreatePinnedToCore(mqtt_app_start, "mqtt_app_start", configMINIMAL_STACK_SIZE + 4096, NULL, MQTT_TASK_PRIORITY, NULL, PRO_CORE);
        xTaskCreatePinnedToCore(system_health_task, "system_health_task", configMINIMAL_STACK_SIZE + 1024, NULL, HEALTH_TASK_PRIORITY, NULL, APP_CORE);
    }


}