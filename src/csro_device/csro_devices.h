#ifndef CSRO_DEVICES_H_
#define CSRO_DEVICES_H_

#include "./csro_common/csro_common.h"

//csro_devices.c
void csro_device_init(void);
void csro_device_on_connect(esp_mqtt_event_handle_t event);
void csro_device_on_message(esp_mqtt_event_handle_t event);

//csro_nlight.c
void csro_nlight_init(void);
void csro_nlight_on_connect(esp_mqtt_event_handle_t event);
void csro_nlight_on_message(esp_mqtt_event_handle_t event);

//csro_nlight_4k4r.c
void csro_nlight_4k4r_init(void);
void csro_nlight_4k4r_on_connect(esp_mqtt_event_handle_t event);
void csro_nlight_4k4r_on_message(esp_mqtt_event_handle_t event);

//csro_nlight_6k4r.c
void csro_nlight_6k4r_init(void);
void csro_nlight_6k4r_on_connect(esp_mqtt_event_handle_t event);
void csro_nlight_6k4r_on_message(esp_mqtt_event_handle_t event);

//csro_dlight.c
void csro_dlight_init(void);
void csro_dlight_on_connect(esp_mqtt_event_handle_t event);
void csro_dlight_on_message(esp_mqtt_event_handle_t event);

//csro_rgblight.c
void csro_rgblight_init(void);
void csro_rgblight_on_connect(esp_mqtt_event_handle_t event);
void csro_rgblight_on_message(esp_mqtt_event_handle_t event);

//csro_airmon.c
void csro_airmon_init(void);
void csro_airmon_on_connect(esp_mqtt_event_handle_t event);
void csro_airmon_on_message(esp_mqtt_event_handle_t event);

#endif