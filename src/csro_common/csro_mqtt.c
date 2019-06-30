#include "csro_common.h"

static int udp_sock;
static TaskHandle_t udp_task_handle;
static TaskHandle_t mqtt_task_handle;

static void timer_task(void *args)
{
    static int count = 0;
    while (true)
    {
        vTaskDelay(1000 / portTICK_RATE_MS);
        time(&sysinfo.time_now);
        localtime_r(&sysinfo.time_now, &sysinfo.time_info);
        strftime(sysinfo.time_str, sizeof(sysinfo.time_str), "%Y-%m-%d %H:%M:%S", &sysinfo.time_info);
        if (sysinfo.time_info.tm_year < (2018 - 1900))
        {
            count++;
        }
        else
        {
            if (sysinfo.time_sync == false)
            {
                sysinfo.time_start = sysinfo.time_now - count;
                sysinfo.time_sync = true;
            }
        }
    }
    vTaskDelete(NULL);
}

static void prepare_mqtt_client_info(void)
{
#ifdef NLIGHT
    sprintf(sysinfo.dev_type, "nlight%d", NLIGHT);
#elif defined DLIGHT
    sprintf(sysinfo.dev_type, "dlight");
#elif defined RGBLIGHT
    sprintf(sysinfo.dev_type, "rgblight");
#elif defined MOTOR
    sprintf(sysinfo.dev_type, "motor%d", MOTOR);
#elif defined AIR_MONITOR
    sprintf(sysinfo.dev_type, "airmon");
#endif

    size_t len = 0;
    nvs_handle handle;
    nvs_open("system", NVS_READONLY, &handle);
    nvs_get_str(handle, "router_ssid", NULL, &len);
    nvs_get_str(handle, "router_ssid", sysinfo.router_ssid, &len);
    nvs_get_str(handle, "router_pass", NULL, &len);
    nvs_get_str(handle, "router_pass", sysinfo.router_pass, &len);
    nvs_close(handle);

    esp_wifi_get_mac(WIFI_MODE_STA, sysinfo.mac);
    sprintf(sysinfo.mac_str, "%02x%02x%02x%02x%02x%02x", sysinfo.mac[0], sysinfo.mac[1], sysinfo.mac[2], sysinfo.mac[3], sysinfo.mac[4], sysinfo.mac[5]);
    sprintf(sysinfo.host_name, "CSRO_%s", sysinfo.mac_str);

    sprintf(mqttinfo.id, "csro/%s", sysinfo.mac_str);
    sprintf(mqttinfo.name, "csro/%s/%s", sysinfo.dev_type, sysinfo.mac_str);
    sprintf(mqttinfo.pass, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", sysinfo.mac[1], sysinfo.mac[3], sysinfo.mac[5], sysinfo.mac[0], sysinfo.mac[2], sysinfo.mac[4], sysinfo.mac[5], sysinfo.mac[3], sysinfo.mac[1], sysinfo.mac[4], sysinfo.mac[2], sysinfo.mac[0]);
    printf("\r\nid = %s.\nname = %s.\npass = %s.\r\nssid = %s.\r\npass = %s.\r\n", mqttinfo.id, mqttinfo.name, mqttinfo.pass, sysinfo.router_ssid, sysinfo.router_pass);
}

static bool create_udp_server(void)
{
    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_sock < 0)
    {
        return false;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(udp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(udp_sock);
        return false;
    }
    return true;
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    if (event->event_id == MQTT_EVENT_CONNECTED)
    {
        printf("MQTT Connected.\r\n");
    }
    else if (event->event_id == MQTT_EVENT_DATA)
    {
        printf("MQTT Got Message.\r\n");
    }
    return ESP_OK;
}

static void mqtt_client_start(void)
{
    sprintf(mqttinfo.lwt_topic, "csro/%s/%s/available", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(mqttinfo.uri, "mqtt://%s", mqttinfo.broker);

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

static void print_task(void *args)
{
    while (true)
    {
        if (sysinfo.time_info.tm_year >= (2018 - 1900))
        {
            printf("Free heap: %d, time is: %s, run %d seconds\n", esp_get_free_heap_size(), sysinfo.time_str, (int)(sysinfo.time_now - sysinfo.time_start));
        }
        else
        {
            printf("Free heap size: %d\n", esp_get_free_heap_size());
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

static void udp_task(void *args)
{
    static char udp_rx_buffer[100];
    while (true)
    {
        bool sock_status = false;
        while (sock_status == false)
        {
            vTaskDelay(1000 / portTICK_RATE_MS);
            sock_status = create_udp_server();
        }
        while (true)
        {
            struct sockaddr_in source_addr;
            socklen_t socklen = sizeof(source_addr);
            bzero(udp_rx_buffer, 100);
            int len = recvfrom(udp_sock, udp_rx_buffer, sizeof(udp_rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            if (len < 0)
            {
                break;
            }
            printf("Udp message %s.\r\n", udp_rx_buffer);
            cJSON *json = cJSON_Parse(udp_rx_buffer);
            cJSON *server;
            if (json != NULL)
            {
                server = cJSON_GetObjectItem(json, "server");
                if ((server != NULL) && (server->type == cJSON_String) && strlen(server->valuestring) >= 7)
                {
                    if (strcmp((char *)server->valuestring, (char *)mqttinfo.broker) != 0)
                    {
                        strcpy((char *)mqttinfo.broker, (char *)server->valuestring);
                        mqtt_client_start();
                    }
                }
            }
            cJSON_Delete(json);
            //vTaskDelay(10000 / portTICK_RATE_MS);
        }
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
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, "pool.ntp.org");
        sntp_init();
        setenv("TZ", "CST-8", 1);
        tzset();
        if (udp_task_handle == NULL)
        {
            xTaskCreate(udp_task, "udp_task", 4096, NULL, configMAX_PRIORITIES - 5, &udp_task_handle);
        }
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
    xTaskCreate(timer_task, "timer_task", 2048, NULL, configMAX_PRIORITIES - 10, NULL);
    xTaskCreate(print_task, "print_task", 2048, NULL, configMAX_PRIORITIES - 11, NULL);
    prepare_mqtt_client_info();
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