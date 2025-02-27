/********************************************
 * ESP32-C6 OTA Update Implementation
 * 
 * This implementation provides:
 * 1. WiFi connection
 * 2. MQTT client for update notifications
 * 3. HTTPS client for firmware download
 * 4. OTA update mechanism with rollback
 ********************************************/

/* Include necessary headers */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "cJSON.h"

/* Configuration defines - Change these for your environment */
#define WIFI_SSID               "YourWiFiSSID"
#define WIFI_PASSWORD           "YourWiFiPassword"
#define MQTT_BROKER_URL         "mqtt://your-mqtt-broker:1883"
#define MQTT_USERNAME           "your-mqtt-username"
#define MQTT_PASSWORD           "your-mqtt-password"
#define MQTT_UPDATE_TOPIC       "device/updates"
#define FIRMWARE_SERVER_URL     "https://your-firmware-server.com"
#define OTA_RECV_TIMEOUT        5000
#define MAX_HTTP_RECV_BUFFER    512
#define MAX_OTA_ATTEMPTS        3

/* FreeRTOS event group to signal WiFi connection status */
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

/* Global variables */
static const char *TAG = "OTA_UPDATE";
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool ota_update_in_progress = false;
static int ota_attempts = 0;
static char fw_upgrade_url[256];

/* Certificate for HTTPS OTA - Update with your server's CA certificate */
extern const char server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const char server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

/* Function prototypes */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, 
                              int32_t event_id, void *event_data);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                              int32_t event_id, void *event_data);
static void mqtt_app_start(void);
static esp_err_t validate_image_header(esp_app_desc_t *new_app_info);
static void ota_task(void *pvParameter);
static void process_ota_update_message(const char *message, int length);

/* WiFi event handler */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (ota_attempts < MAX_OTA_ATTEMPTS) {
            esp_wifi_connect();
            ESP_LOGI(TAG, "Retry connecting to WiFi...");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        ota_attempts = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        
        // Start MQTT client after WiFi connection
        mqtt_app_start();
    }
}

/* Connect to WiFi */
static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Wait for connection or failure */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi network: %s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi network: %s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Unexpected WiFi connection event");
    }
}

/* MQTT event handler */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                              int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            esp_mqtt_client_subscribe(client, MQTT_UPDATE_TOPIC, 0);
            ESP_LOGI(TAG, "Subscribed to topic: %s", MQTT_UPDATE_TOPIC);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT Subscribed, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT Unsubscribed, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT Published, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT Data Received");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
            
            if (strncmp(event->topic, MQTT_UPDATE_TOPIC, event->topic_len) == 0) {
                process_ota_update_message(event->data, event->data_len);
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Error");
            break;
            
        default:
            ESP_LOGI(TAG, "MQTT Other event id: %d", event_id);
            break;
    }
}

/* Initialize MQTT client */
static void mqtt_app_start(void)
{
    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    
    // Register event handler
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // Start MQTT client
    esp_mqtt_client_start(mqtt_client);
}

/* Process update messages from MQTT */
static void process_ota_update_message(const char *message, int length)
{
    if (ota_update_in_progress) {
        ESP_LOGI(TAG, "OTA update already in progress, ignoring update message");
        return;
    }

    cJSON *root = cJSON_ParseWithLength(message, length);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse update message");
        return;
    }

    // Extract version and download URL
    cJSON *version = cJSON_GetObjectItem(root, "version");
    cJSON *url = cJSON_GetObjectItem(root, "url");
    
    if (!cJSON_IsString(version) || !cJSON_IsString(url)) {
        ESP_LOGE(TAG, "Invalid update message format");
        cJSON_Delete(root);
        return;
    }

    // Compare versions to check if update is needed
    const esp_app_desc_t *running_app_info = esp_app_get_description();
    
    ESP_LOGI(TAG, "Current firmware version: %s", running_app_info->version);
    ESP_LOGI(TAG, "Available firmware version: %s", version->valuestring);
    
    if (strcmp(version->valuestring, running_app_info->version) == 0) {
        ESP_LOGI(TAG, "Current version is up to date");
        cJSON_Delete(root);
        return;
    }
    
    // Prepare for update
    strlcpy(fw_upgrade_url, url->valuestring, sizeof(fw_upgrade_url));
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Starting OTA update from: %s", fw_upgrade_url);
    ota_update_in_progress = true;
    
    // Start OTA task
    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
}

/* Validate firmware header before update */
static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    // Get current firmware info
    const esp_app_desc_t *running_app_info = esp_app_get_description();
    
    // Validate project name
    if (strcmp(new_app_info->project_name, running_app_info->project_name) != 0) {
        ESP_LOGW(TAG, "OTA firmware is from a different project: %s (expected: %s)",
                 new_app_info->project_name, running_app_info->project_name);
        return ESP_ERR_INVALID_VERSION;
    }
    
    ESP_LOGI(TAG, "Running firmware version: %s", running_app_info->version);
    ESP_LOGI(TAG, "New firmware version: %s", new_app_info->version);
    
    return ESP_OK;
}

/* HTTP event handler for HTTPS OTA */
esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            // For large downloads, don't print all data
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

/* OTA update task */
static void ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA task");
    
    // Configure OTA
    esp_http_client_config_t config = {
        .url = fw_upgrade_url,
        .cert_pem = server_cert_pem_start,
        .event_handler = http_event_handler,
        .timeout_ms = OTA_RECV_TIMEOUT,
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        goto ota_end;
    }
    
    // Get firmware image header and validate
    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
        goto ota_end;
    }
    
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Image validation failed");
        goto ota_end;
    }
    
    // Perform OTA update
    ESP_LOGI(TAG, "Starting OTA update...");
    
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        // Print download progress
        int progress = esp_https_ota_get_image_len_read(https_ota_handle) * 100 / 
                       esp_https_ota_get_image_size(https_ota_handle);
        ESP_LOGI(TAG, "OTA progress: %d%%", progress);
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA upgrade failed: %s", esp_err_to_name(err));
        goto ota_end;
    }
    
    // Finish OTA update
    err = esp_https_ota_finish(https_ota_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "ESP HTTPS OTA upgrade failed: %s", esp_err_to_name(err));
        }
        goto ota_end;
    }
    
    ESP_LOGI(TAG, "OTA update successful! Rebooting...");
    esp_restart();
    
ota_end:
    ota_update_in_progress = false;
    if (https_ota_handle) {
        esp_https_ota_abort(https_ota_handle);
    }
    vTaskDelete(NULL);
}

/* Setup rollback functionality */
static void check_rollback(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    // Get OTA state
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // This means we've just been updated and need to verify if we're stable
            ESP_LOGI(TAG, "First boot after OTA update, marking as valid");
            esp_ota_mark_app_valid_cancel_rollback();
        } else {
            ESP_LOGI(TAG, "Running from partition: %s (state: %d)", running->label, ota_state);
        }
    }
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Check if we need to confirm a successful update
    check_rollback();
    
    // Print current firmware info
    const esp_app_desc_t *app_desc = esp_app_get_description();
    ESP_LOGI(TAG, "Running firmware:");
    ESP_LOGI(TAG, "  Project name: %s", app_desc->project_name);
    ESP_LOGI(TAG, "  Version: %s", app_desc->version);
    ESP_LOGI(TAG, "  Compile time: %s %s", app_desc->date, app_desc->time);
    
    // Connect to WiFi
    wifi_init_sta();
}