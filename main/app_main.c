#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "dht.h"

#define WIFI_SSID       "Toan2.4"
#define WIFI_PASSWORD   "2580225802TpXn#"
#define MQTT_URI        "mqtt.flespi.io"
#define MQTT_USERNAME   "8UfmqqhmM4rC04D0mU4gfocqPDluQ93OR9Y5n5fZXxrOrUEBDwIIKKXpb8HR6AvL"
#define MQTT_PASSWORD   ""
#define MQTT_TOPIC_TEMP     "/temperature"
#define MQTT_TOPIC_HUMI     "/humidity"

#define SENSOR_TYPE DHT_TYPE_DHT11

uint32_t MQTT_CONNECTED = 0;

static const char *TAG = "mqtt_dht";


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            MQTT_CONNECTED = 1;
            msg_id = esp_mqtt_client_subscribe(client, MQTT_TOPIC_TEMP, 0);
            ESP_LOGI(TAG, "Subscribed to topic '%s'", MQTT_TOPIC_TEMP);
            msg_id = esp_mqtt_client_subscribe(client, MQTT_TOPIC_HUMI, 0);
            ESP_LOGI(TAG, "Subscribed to topic '%s'", MQTT_TOPIC_HUMI);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            MQTT_CONNECTED=0;
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

esp_mqtt_client_handle_t client = NULL;
static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

static void dht_task(void *params)
{
    float temperature = 0.0;
    float humidity = 0.0;

    while (1) 
    {
        if (dht_read_float_data(SENSOR_TYPE, 4, &temperature, &humidity) == ESP_OK) 
        {
            char payload_temp[10];
            char payload_humi[10];
            snprintf(payload_temp, sizeof(payload_temp), "%.2f", temperature);
            snprintf(payload_humi, sizeof(payload_humi), "%.2f", humidity);

            if(MQTT_CONNECTED)
            {
                esp_mqtt_client_publish(client, MQTT_TOPIC_TEMP, payload_temp, strlen(payload_temp), 0, 0);
                ESP_LOGI(TAG, "Published temperature: %s to topic '%s'", payload_temp, MQTT_TOPIC_TEMP);
                esp_mqtt_client_publish(client, MQTT_TOPIC_HUMI, payload_humi, strlen(payload_humi), 0, 0);
                ESP_LOGI(TAG, "Published humidity: %s to topic '%s'", payload_humi, MQTT_TOPIC_HUMI);
            } else {
            ESP_LOGE(TAG, "Could not read data from DHT sensor");
            }
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    // Start MQTT
    mqtt_app_start();

    // Start DHT task
    xTaskCreate(dht_task, "dht_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
}
