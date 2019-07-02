#include "csro_devices.h"

#ifdef NLIGHT

int light_state[3];

static void nlight_relay_led_task(void *pvParameters)
{
    while (true)
    {
        vTaskDelay(20 / portTICK_RATE_MS);
        for (size_t i = 0; i < 3; i++)
        {
            if (light_state[i] == 1)
            {
            }
            else
            {
            }
        }
    }
    vTaskDelete(NULL);
}

void csro_nlight_init(void)
{
    xTaskCreate(nlight_relay_led_task, "nlight_relay_led_task", 2048, NULL, configMAX_PRIORITIES - 8, NULL);
}

void csro_update_nlight_state(void)
{
    cJSON *state_json = cJSON_CreateObject();
    cJSON_AddItemToObject(state_json, "state", cJSON_CreateIntArray(light_state, 3));
    cJSON_AddNumberToObject(state_json, "run", sysinfo.time_run);
    char *out = cJSON_PrintUnformatted(state_json);
    strcpy(mqttinfo.content, out);
    free(out);
    cJSON_Delete(state_json);
    sprintf(mqttinfo.pub_topic, "csro/%s/%s/state", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_publish(mqttclient, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);
}

void csro_nlight_on_connect(esp_mqtt_event_handle_t event)
{
    sprintf(mqttinfo.sub_topic, "csro/%s/%s/set/#", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_subscribe(event->client, mqttinfo.sub_topic, 1);

    for (size_t i = 0; i < NLIGHT; i++)
    {
        char prefix[50], name[50], state[50], command[50];
        sprintf(mqttinfo.pub_topic, "csro/light/%s_%s_%d/config", sysinfo.mac_str, sysinfo.dev_type, i + 1);
        sprintf(prefix, "csro/%s/%s", sysinfo.mac_str, sysinfo.dev_type);
        sprintf(name, "%s_%d_%s", sysinfo.dev_type, i + 1, sysinfo.mac_str);
        sprintf(state, "{{value_json.state[%d]}}", i);
        sprintf(command, "~/set/%d", i + 1);

        cJSON *config_json = cJSON_CreateObject();
        cJSON_AddStringToObject(config_json, "~", prefix);
        cJSON_AddStringToObject(config_json, "name", name);
        cJSON_AddStringToObject(config_json, "avty_t", "~/available");
        cJSON_AddStringToObject(config_json, "pl_avail", "online");
        cJSON_AddStringToObject(config_json, "pl_not_avail", "offline");
        cJSON_AddStringToObject(config_json, "stat_t", "~/state");
        cJSON_AddStringToObject(config_json, "stat_val_tpl", state);
        cJSON_AddStringToObject(config_json, "cmd_t", command);
        cJSON_AddStringToObject(config_json, "opt", "false");
        cJSON_AddNumberToObject(config_json, "pl_on", 1);
        cJSON_AddNumberToObject(config_json, "pl_off", 0);
        cJSON_AddNumberToObject(config_json, "qos", 1);
        char *out = cJSON_PrintUnformatted(config_json);
        strcpy(mqttinfo.content, out);
        free(out);
        cJSON_Delete(config_json);
        esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, mqttinfo.content, 0, 1, 1);
    }
    sprintf(mqttinfo.pub_topic, "csro/%s/%s/available", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, "online", 0, 1, 1);
    csro_update_nlight_state();
}

void csro_nlight_on_message(esp_mqtt_event_handle_t event)
{
    bool update = false;
    char topic[50];
    for (size_t i = 1; i < NLIGHT + 1; i++)
    {
        sprintf(topic, "csro/%s/%s/set/%d", sysinfo.mac_str, sysinfo.dev_type, i);
        if (strncmp(topic, event->topic, event->topic_len) == 0)
        {
            if (strncmp("0", event->data, event->data_len) == 0)
            {
                light_state[i - 1] = 0;
                update = true;
            }
            else if (strncmp("1", event->data, event->data_len) == 0)
            {
                light_state[i - 1] = 1;
                update = true;
            }
        }
    }
    if (update)
    {
        csro_update_nlight_state();
    }
}

#endif