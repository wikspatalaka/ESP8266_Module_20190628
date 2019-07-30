#include "csro_devices.h"

void csro_device_init(void)
{
#ifdef NLIGHT
    csro_nlight_init();
#elif defined NLIGHT_4K4R
    csro_nlight_4k4r_init();
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

void csro_device_on_connect(esp_mqtt_event_handle_t event)
{
#ifdef NLIGHT
    csro_nlight_on_connect(event);
#elif defined NLIGHT_4K4R
    csro_nlight_4k4r_on_connect(event);
#elif defined DLIGHT
    csro_dlight_on_connect(event);
#elif defined RGBLIGHT
    csro_rgblight_on_connect(event);
#elif defined MOTOR
    csro_motor_on_connect(event);
#elif defined AIR_MONITOR
    csro_airmon_on_connect(event);
#endif
}

void csro_device_on_message(esp_mqtt_event_handle_t event)
{
#ifdef NLIGHT
    csro_nlight_on_message(event);
#elif defined NLIGHT_4K4R
    csro_nlight_4k4r_on_message(event);
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