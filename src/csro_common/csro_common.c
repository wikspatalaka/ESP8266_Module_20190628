#include "csro_common.h"
#include "./csro_device/csro_devices.h"

csro_system sysinfo;
csro_mqtt mqttinfo;
esp_mqtt_client_handle_t mqttclient;

void prepare_mqtt_client_info(void)
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

    sprintf(mqttinfo.lwt_topic, "csro/%s/%s/available", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(mqttinfo.id, "csro/%s", sysinfo.mac_str);
    sprintf(mqttinfo.name, "csro/%s/%s", sysinfo.dev_type, sysinfo.mac_str);
    sprintf(mqttinfo.pass, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", sysinfo.mac[1], sysinfo.mac[3], sysinfo.mac[5], sysinfo.mac[0], sysinfo.mac[2], sysinfo.mac[4], sysinfo.mac[5], sysinfo.mac[3], sysinfo.mac[1], sysinfo.mac[4], sysinfo.mac[2], sysinfo.mac[0]);
    printf("\r\nid = %s.\nname = %s.\npass = %s.\r\nssid = %s.\r\npass = %s.\r\n", mqttinfo.id, mqttinfo.name, mqttinfo.pass, sysinfo.router_ssid, sysinfo.router_pass);
}

void app_main(void)
{
    nvs_handle handle;
    nvs_flash_init();
    nvs_open("system", NVS_READWRITE, &handle);
    nvs_get_u32(handle, "power_cnt", &sysinfo.power_cnt);
    nvs_set_u32(handle, "power_cnt", (sysinfo.power_cnt + 1));
    nvs_get_u8(handle, "router_flag", &sysinfo.router_flag);
    nvs_commit(handle);
    nvs_close(handle);
    csro_device_init();
    if (sysinfo.router_flag == 1)
    {
        csro_start_mqtt();
    }
    else
    {
        csro_start_smart_config();
    }
}