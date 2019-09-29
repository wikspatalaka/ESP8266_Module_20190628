#include "csro_devices.h"

#ifdef RGBLIGHT

#define RGBLIGHT_PWM_PERIOD 2550

typedef struct
{
    uint8_t state;
    uint8_t bright;
    uint8_t rgb[3];
} csro_rgb_light;

csro_rgb_light rgb_light;
const uint32_t rgb_pin[3] = {15, 13, 12};
uint32_t rgb_duties[3] = {0, 0, 0};
int16_t rgb_phases[3] = {0, 0, 0};

static void csro_update_rgblight_state(void)
{
    cJSON *state_json = cJSON_CreateObject();
    cJSON *rgbstate;
    int rgb_value[3];

    cJSON_AddItemToObject(state_json, "state", rgbstate = cJSON_CreateObject());
    cJSON_AddNumberToObject(rgbstate, "on", rgb_light.state);
    cJSON_AddNumberToObject(rgbstate, "bright", rgb_light.bright);
    rgb_value[0] = rgb_light.rgb[0];
    rgb_value[1] = rgb_light.rgb[1];
    rgb_value[2] = rgb_light.rgb[2];
    cJSON_AddItemToObject(rgbstate, "rgb", cJSON_CreateIntArray(rgb_value, 3));
    cJSON_AddStringToObject(state_json, "time", sysinfo.time_str);
    cJSON_AddNumberToObject(state_json, "run", sysinfo.time_run);
    char *out = cJSON_PrintUnformatted(state_json);
    strcpy(mqttinfo.content, out);
    free(out);
    cJSON_Delete(state_json);
    sprintf(mqttinfo.pub_topic, "csro/%s/%s/state", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_publish(mqttclient, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);
}

static void rgb_light_pwm_task(void *pvParameters)
{
    uint32_t duty[3];
    while (true)
    {
        bool update_pwm = false;
        if (rgb_light.state == 0)
        {
            for (size_t i = 0; i < 3; i++)
            {
                pwm_get_duty(i, &duty[i]);
                if (duty[i] != 0 && rgb_light.rgb[i] > 0)
                {
                    if (duty[i] >= rgb_light.rgb[i])
                    {
                        pwm_set_duty(i, duty[i] - rgb_light.rgb[i]);
                    }
                    else if (duty[i] > 0)
                    {
                        pwm_set_duty(i, 0);
                    }
                    update_pwm = true;
                }
                else if (duty[i] != 0 && rgb_light.rgb[i] == 0)
                {
                    pwm_set_duty(i, 0);
                    update_pwm = true;
                }
            }
        }
        else if (rgb_light.state == 1)
        {
            for (size_t i = 0; i < 3; i++)
            {
                pwm_get_duty(i, &duty[i]);
                if (duty[i] > rgb_light.rgb[i] * rgb_light.bright)
                {
                    if (duty[i] - (rgb_light.rgb[i] * rgb_light.bright) >= 50)
                    {
                        pwm_set_duty(i, duty[i] - 50);
                    }
                    else
                    {
                        pwm_set_duty(i, rgb_light.rgb[i] * rgb_light.bright);
                    }
                    update_pwm = true;
                }
                else if (duty[i] < rgb_light.rgb[i] * rgb_light.bright)
                {
                    if ((rgb_light.rgb[i] * rgb_light.bright) - duty[i] >= 50)
                    {
                        pwm_set_duty(i, duty[i] + 50);
                    }
                    else
                    {
                        pwm_set_duty(i, rgb_light.rgb[i] * rgb_light.bright);
                    }
                    update_pwm = true;
                }
            }
        }
        if (update_pwm)
        {
            pwm_start();
        }
        vTaskDelay(15 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void csro_rgblight_init(void)
{
    pwm_init(RGBLIGHT_PWM_PERIOD, rgb_duties, 3, rgb_pin);
    pwm_set_channel_invert(0x00);
    pwm_set_phases(rgb_phases);
    pwm_start();
    xTaskCreate(rgb_light_pwm_task, "rgb_light_pwm_task", 2048, NULL, configMAX_PRIORITIES - 9, NULL);
}

void csro_rgblight_on_connect(esp_mqtt_event_handle_t event)
{
    sprintf(mqttinfo.sub_topic, "csro/%s/%s/set/#", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_subscribe(event->client, mqttinfo.sub_topic, 0);

    char prefix[50], name[50];
    sprintf(mqttinfo.pub_topic, "csro/light/%s_%s/config", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(prefix, "csro/%s/%s", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(name, "%s_%s", sysinfo.dev_type, sysinfo.mac_str);

    cJSON *config_json = cJSON_CreateObject();
    cJSON_AddStringToObject(config_json, "~", prefix);
    cJSON_AddStringToObject(config_json, "name", name);
    cJSON_AddStringToObject(config_json, "avty_t", "~/available");
    cJSON_AddStringToObject(config_json, "pl_avail", "online");
    cJSON_AddStringToObject(config_json, "pl_not_avail", "offline");
    cJSON_AddStringToObject(config_json, "stat_t", "~/state");
    cJSON_AddStringToObject(config_json, "stat_val_tpl", "{{value_json.state.on}}");
    cJSON_AddStringToObject(config_json, "bri_stat_t", "~/state");
    cJSON_AddStringToObject(config_json, "bri_val_tpl", "{{value_json.state.bright}}");
    cJSON_AddNumberToObject(config_json, "bri_scl", 10);
    cJSON_AddStringToObject(config_json, "cmd_t", "~/set");
    cJSON_AddNumberToObject(config_json, "pl_on", 1);
    cJSON_AddNumberToObject(config_json, "pl_off", 0);
    cJSON_AddStringToObject(config_json, "bri_cmd_t", "~/set/bright");
    cJSON_AddStringToObject(config_json, "rgb_cmd_t", "~/set/rgb");
    cJSON_AddStringToObject(config_json, "rgb_stat_t", "~/state");
    cJSON_AddStringToObject(config_json, "rgb_val_tpl", "{{value_json.state.rgb}}");
    cJSON_AddStringToObject(config_json, "opt", "false");
    cJSON_AddNumberToObject(config_json, "qos", 0);

    char *out = cJSON_PrintUnformatted(config_json);
    strcpy(mqttinfo.content, out);
    free(out);
    cJSON_Delete(config_json);
    esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);

    sprintf(mqttinfo.pub_topic, "csro/%s/%s/available", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, "online", 0, 0, 1);
    csro_update_rgblight_state();
}

void csro_rgblight_on_message(esp_mqtt_event_handle_t event)
{
    bool update = false;
    char set_state_topic[50];
    char set_bright_topic[50];
    char set_rgb_topic[50];

    sprintf(set_state_topic, "csro/%s/%s/set", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(set_bright_topic, "csro/%s/%s/set/bright", sysinfo.mac_str, sysinfo.dev_type);
    sprintf(set_rgb_topic, "csro/%s/%s/set/rgb", sysinfo.mac_str, sysinfo.dev_type);
    if (event->data_len == 1 && (strncmp(set_state_topic, event->topic, event->topic_len) == 0))
    {
        if (strncmp("0", event->data, event->data_len) == 0)
        {
            rgb_light.state = 0;
            update = true;
        }
        else if (strncmp("1", event->data, event->data_len) == 0)
        {
            if (rgb_light.rgb[0] == 0 && rgb_light.rgb[1] == 0 && rgb_light.rgb[2] == 0)
            {
                rgb_light.rgb[0] = 255;
                rgb_light.rgb[1] = 255;
                rgb_light.rgb[2] = 255;
            }
            rgb_light.state = 1;
            update = true;
        }
    }
    else if ((event->data_len <= 3) && (strncmp(set_bright_topic, event->topic, event->topic_len) == 0))
    {
        char payload[5] = {0};
        strncpy(payload, event->data, event->data_len);
        uint32_t data_number;
        sscanf(payload, "%d", &data_number);
        if (data_number <= 10)
        {
            rgb_light.bright = data_number;
            rgb_light.state = 1;
            update = true;
        }
    }
    else if ((event->data_len <= 12) && (strncmp(set_rgb_topic, event->topic, event->topic_len) == 0))
    {
        char payload[15] = {0};
        strncpy(payload, event->data, event->data_len);
        uint32_t values[3];
        sscanf(payload, "%d,%d,%d", &values[0], &values[1], &values[2]);
        for (size_t i = 0; i < 3; i++)
        {
            if (values[i] <= 255)
            {
                rgb_light.rgb[i] = values[i];
                update = true;
            }
        }
    }
    if (update)
    {
        csro_update_rgblight_state();
    }
}

#endif
