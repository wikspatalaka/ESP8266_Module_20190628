#include "csro_common.h"

static void smartconfig_callback(smartconfig_status_t status, void *pdata)
{
    if (status == SC_STATUS_LINK)
    {
        wifi_config_t *wifi_config = pdata;
        strcpy(sysinfo.router_ssid, (const char *)wifi_config->sta.ssid);
        strcpy(sysinfo.router_pass, (const char *)wifi_config->sta.password);
        esp_wifi_disconnect();
        esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config);
        esp_wifi_connect();
    }
    else if (status == SC_STATUS_LINK_OVER)
    {
        esp_smartconfig_stop();
        nvs_handle handle;
        nvs_open("system", NVS_READWRITE, &handle);
        nvs_set_str(handle, "router_ssid", sysinfo.router_ssid);
        nvs_set_str(handle, "router_pass", sysinfo.router_pass);
        nvs_set_u8(handle, "router_flag", 1);
        nvs_commit(handle);
        nvs_close(handle);
        esp_restart();
    }
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    if (event->event_id == SYSTEM_EVENT_STA_START)
    {
        esp_smartconfig_set_type(SC_TYPE_AIRKISS);
        esp_smartconfig_start(smartconfig_callback);
    }
    else if (event->event_id == SYSTEM_EVENT_STA_DISCONNECTED)
    {
        esp_smartconfig_stop();
        esp_smartconfig_start(smartconfig_callback);
    }
    return ESP_OK;
}

void csro_start_smart_config(void)
{
    tcpip_adapter_init();
    esp_event_loop_init(event_handler, NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
}