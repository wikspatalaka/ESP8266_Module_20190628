#include "csro_common.h"
#include "./csro_device/csro_devices.h"

csro_system sysinfo;
csro_mqtt mqttinfo;
esp_mqtt_client_handle_t mqttclient;

static void system_init(void)
{
    nvs_handle handle;
    nvs_flash_init();
    nvs_open("system", NVS_READWRITE, &handle);
    nvs_get_u32(handle, "power_cnt", &sysinfo.power_cnt);
    nvs_set_u32(handle, "power_cnt", (sysinfo.power_cnt + 1));
    nvs_get_u8(handle, "router_flag", &sysinfo.router_flag);
    nvs_commit(handle);
    nvs_close(handle);
}

void app_main(void)
{
    system_init();
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