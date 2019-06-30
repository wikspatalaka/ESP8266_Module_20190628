#include "csro_devices.h"

void csro_device_init(void)
{
#ifdef NLIGHT
    csro_nlight_init();
#elif defined DLIGHT
    csro_dlight_init();
#elif defined RGBLIGHT
    csro_rgblight_init();
#elif defined MOTOR
    csro_motor_init();
#elif defined AIR_MONITOR
    csro_airmon_init();
#endif
}

void csro_device_on_connect(esp_mqtt_client_handle_t client)
{
#ifdef NLIGHT
    csro_nlight_on_connect(client);
#elif defined DLIGHT
    csro_dlight_on_connect(client);
#elif defined RGBLIGHT
    csro_rgblight_on_connect(client);
#elif defined MOTOR
    csro_motor_on_connect(client);
#elif defined AIR_MONITOR
    csro_airmon_on_connect(client);
#endif
}

void csro_device_on_message(esp_mqtt_event_handle_t event)
{
#ifdef NLIGHT
    csro_nlight_on_message(event);
#elif defined DLIGHT
    csro_dlight_on_message(event);
#elif defined RGBLIGHT
    csro_rgblight_on_message(event);
#elif defined MOTOR
    csro_motor_on_message(event);
#elif defined AIR_MONITOR
    csro_airmon_on_message(event);
#endif
}