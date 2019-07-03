#include "csro_common.h"
#include "csro_device/csro_devices.h"

static void time_stamp_task(void *args)
{
    while (true)
    {
        printf("Run %d mins. Free heap is %d\r\n", sysinfo.time_run, esp_get_free_heap_size());
        vTaskDelay(60000 / portTICK_RATE_MS);
        sysinfo.time_run++;
    }
    vTaskDelete(NULL);
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    if (event->event_id == MQTT_EVENT_CONNECTED)
    {
        csro_device_on_connect(event);
    }
    else if (event->event_id == MQTT_EVENT_DATA)
    {
        csro_device_on_message(event);
    }
    return ESP_OK;
}

static void udp_receive_mqtt_server(void)
{
    static char udp_buffer[256];
    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_sock < 0)
    {
        return;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(udp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(udp_sock);
        return;
    }
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    bzero(udp_buffer, 256);
    int len = recvfrom(udp_sock, udp_buffer, sizeof(udp_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
    close(udp_sock);
    if (len < 0)
    {
        return;
    }
    cJSON *json = cJSON_Parse(udp_buffer);
    cJSON *server;
    if (json != NULL)
    {
        server = cJSON_GetObjectItem(json, "server");
        if ((server != NULL) && (server->type == cJSON_String) && strlen(server->valuestring) >= 7)
        {
            if (strcmp((char *)server->valuestring, (char *)mqttinfo.broker) != 0)
            {
                strcpy((char *)mqttinfo.broker, (char *)server->valuestring);
                sprintf(mqttinfo.uri, "mqtt://%s", mqttinfo.broker);
                printf("%s\r\n", mqttinfo.uri);
                if (mqttclient != NULL)
                {
                    esp_mqtt_client_destroy(mqttclient);
                }
                esp_mqtt_client_config_t mqtt_cfg = {
                    .event_handle = mqtt_event_handler,
                    .client_id = mqttinfo.id,
                    .username = mqttinfo.name,
                    .password = mqttinfo.pass,
                    .uri = mqttinfo.uri,
                    .keepalive = 60,
                    .lwt_topic = mqttinfo.lwt_topic,
                    .lwt_msg = "offline",
                    .lwt_retain = 1,
                    .lwt_qos = 1,
                };
                mqttclient = esp_mqtt_client_init(&mqtt_cfg);
                esp_mqtt_client_start(mqttclient);
            }
        }
    }
    cJSON_Delete(json);
}

static void udp_server_task(void *args)
{
    while (true)
    {
        udp_receive_mqtt_server();
        vTaskDelay(60000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    system_event_info_t *info = &event->event_info;
    if (event->event_id == SYSTEM_EVENT_STA_START)
    {
        tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, sysinfo.host_name);
        esp_wifi_connect();
    }
    else if (event->event_id == SYSTEM_EVENT_STA_GOT_IP)
    {
    }
    else if (event->event_id == SYSTEM_EVENT_STA_DISCONNECTED)
    {
        if (info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT)
        {
            esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N);
        }
        esp_wifi_connect();
    }
    return ESP_OK;
}

void csro_start_mqtt(void)
{
    xTaskCreate(time_stamp_task, "time_stamp_task", 2048, NULL, configMAX_PRIORITIES - 10, NULL);
    xTaskCreate(udp_server_task, "udp_server_task", 2048, NULL, configMAX_PRIORITIES - 7, NULL);

    csro_mqtt_client_info();
    tcpip_adapter_init();
    esp_event_loop_init(wifi_event_handler, NULL);

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&config);

    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, (char *)sysinfo.router_ssid);
    strcpy((char *)wifi_config.sta.password, (char *)sysinfo.router_pass);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}