#include "csro_devices.h"
#include "csro_driver/aw9523b.h"

#ifdef NLIGHT_4K4R

#define KEY_01_NUM GPIO_NUM_0
#define KEY_02_NUM GPIO_NUM_4
#define KEY_03_NUM GPIO_NUM_13
#define KEY_04_NUM GPIO_NUM_5
#define KEY_IR_NUM GPIO_NUM_12
#define GPIO_MASK ((1ULL << KEY_01_NUM) | (1ULL << KEY_02_NUM) | (1ULL << KEY_03_NUM) | (1ULL << KEY_04_NUM) | (1ULL << KEY_IR_NUM))

int light_state[4] = {0, 0, 0, 0};

void csro_update_nlight_4k4r_state(void)
{
    if (mqttclient != NULL)
    {
        cJSON *state_json = cJSON_CreateObject();
        cJSON_AddItemToObject(state_json, "state", cJSON_CreateIntArray(light_state, 4));
        cJSON_AddNumberToObject(state_json, "run", sysinfo.time_run);
        char *out = cJSON_PrintUnformatted(state_json);
        strcpy(mqttinfo.content, out);
        free(out);
        cJSON_Delete(state_json);
        sprintf(mqttinfo.pub_topic, "csro/%s/%s/state", sysinfo.mac_str, sysinfo.dev_type);
        esp_mqtt_client_publish(mqttclient, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);
    }
}

static void nlight_4k4r_relay_led_task(void *args)
{
    static uint8_t status[4];
    while (true)
    {
        bool update = false;
        vTaskDelay(50 / portTICK_RATE_MS);
        for (size_t i = 0; i < 4; i++)
        {
            if (status[i] != light_state[i])
            {
                status[i] = light_state[i];
                update = true;
            }
            csro_set_led(i, light_state[i] == 1 ? 128 : 8);
            csro_set_relay(i, light_state[i] == 1 ? true : false);
        }
        if (update)
        {
            csro_set_vibrator();
            csro_update_nlight_4k4r_state();
        }
    }
    vTaskDelete(NULL);
}

static void nlight_4k4r_key_task(void *args)
{
    static uint32_t holdtime[4];
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_MASK;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    while (true)
    {
        int key_status[4] = {gpio_get_level(KEY_01_NUM), gpio_get_level(KEY_02_NUM), gpio_get_level(KEY_03_NUM), gpio_get_level(KEY_04_NUM)};
        for (size_t i = 0; i < 4; i++)
        {
            if (key_status[i] == 0)
            {
                holdtime[i]++;
                if (holdtime[i] == 2)
                {
                    light_state[i] = !light_state[i];
                }
            }
            else
            {
                holdtime[i] = 0;
            }
        }
        vTaskDelay(20 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void csro_nlight_4k4r_init(void)
{
    csro_aw9523b_init();
    xTaskCreate(nlight_4k4r_relay_led_task, "nlight_4k4r_relay_led_task", 4096, NULL, configMAX_PRIORITIES - 4, NULL);
    xTaskCreate(nlight_4k4r_key_task, "nlight_4k4r_key_task", 2048, NULL, configMAX_PRIORITIES - 6, NULL);
}
void csro_nlight_4k4r_on_connect(esp_mqtt_event_handle_t event)
{
    sprintf(mqttinfo.sub_topic, "csro/%s/%s/set/#", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_subscribe(event->client, mqttinfo.sub_topic, 0);

    for (size_t i = 0; i < 4; i++)
    {
        char prefix[50], name[50], state[50], command[50];
        sprintf(mqttinfo.pub_topic, "csro/light/%s_%s_%d/config", sysinfo.mac_str, sysinfo.dev_type, i);
        sprintf(prefix, "csro/%s/%s", sysinfo.mac_str, sysinfo.dev_type);
        sprintf(name, "%s_%d_%s", sysinfo.dev_type, i, sysinfo.mac_str);
        sprintf(state, "{{value_json.state[%d]}}", i);
        sprintf(command, "~/set/%d", i);

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
        cJSON_AddNumberToObject(config_json, "qos", 0);
        char *out = cJSON_PrintUnformatted(config_json);
        strcpy(mqttinfo.content, out);
        free(out);
        cJSON_Delete(config_json);
        esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);
    }
    sprintf(mqttinfo.pub_topic, "csro/%s/%s/available", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, "online", 0, 0, 1);
    csro_update_nlight_4k4r_state();
}
void csro_nlight_4k4r_on_message(esp_mqtt_event_handle_t event)
{
    char topic[50];
    for (size_t i = 0; i < 4; i++)
    {
        sprintf(topic, "csro/%s/%s/set/%d", sysinfo.mac_str, sysinfo.dev_type, i);
        if (strncmp(topic, event->topic, event->topic_len) == 0)
        {
            if (strncmp("0", event->data, event->data_len) == 0)
            {
                light_state[i] = 0;
            }
            else if (strncmp("1", event->data, event->data_len) == 0)
            {
                light_state[i] = 1;
            }
        }
    }
}

#endif