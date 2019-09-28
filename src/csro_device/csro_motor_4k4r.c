#include "csro_devices.h"
#include "csro_driver/aw9523b.h"

#ifdef MOTOR_4K4R

#define KEY_01_NUM GPIO_NUM_0
#define KEY_02_NUM GPIO_NUM_4
#define KEY_03_NUM GPIO_NUM_13
#define KEY_04_NUM GPIO_NUM_5
#define KEY_IR_NUM GPIO_NUM_12
#define MOTOR1_UP_RELAY 1
#define MOTOR1_DOWN_RELAY 2
#define MOTOR2_UP_RELAY 3
#define MOTOR2_DOWN_RELAY 4

#define GPIO_MASK ((1ULL << KEY_01_NUM) | (1ULL << KEY_02_NUM) | (1ULL << KEY_03_NUM) | (1ULL << KEY_04_NUM) | (1ULL << KEY_IR_NUM))

typedef enum
{
    STOP = 0,
    UP = 1,
    DOWN = 2,
} motor_state;

motor_state motor[2] = {STOP};
int action[2] = {UP, UP};
uint8_t relay_index[4] = {MOTOR1_UP_RELAY, MOTOR2_UP_RELAY, MOTOR1_DOWN_RELAY, MOTOR2_DOWN_RELAY};

void csro_update_motor_4k4r_state(void)
{
    if (mqttclient != NULL)
    {
        cJSON *state_json = cJSON_CreateObject();
        cJSON_AddItemToObject(state_json, "state", cJSON_CreateIntArray(action, 2));
        cJSON_AddNumberToObject(state_json, "run", sysinfo.time_run);
        char *out = cJSON_PrintUnformatted(state_json);
        strcpy(mqttinfo.content, out);
        free(out);
        cJSON_Delete(state_json);
        sprintf(mqttinfo.pub_topic, "csro/%s/%s/state", sysinfo.mac_str, sysinfo.dev_type);
        esp_mqtt_client_publish(mqttclient, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);
    }
}

static void motor_4k4r_relay_led_task(void *args)
{
    static motor_state last_state[2] = {STOP};
    static uint16_t count_200ms[2] = {0};
    while (true)
    {
        bool update = false;
        vTaskDelay(200 / portTICK_RATE_MS);
        for (uint8_t i = 0; i < 2; i++)
        {
            if (last_state[i] != motor[i])
            {
                last_state[i] = motor[i];
                update = true;
                if (motor[i] == UP)
                {
                    action[i] = 1;
                }
                else if (motor[i] == DOWN)
                {
                    action[i] = 0;
                }
                printf("motor %d status %d\r\n", i, motor[i]);
            }
            csro_set_led(i, last_state[i] == UP ? 128 : 8);
            csro_set_led(i + 2, last_state[i] == DOWN ? 128 : 8);

            csro_set_relay(relay_index[i], last_state[i] == UP ? true : false);
            csro_set_relay(relay_index[i + 2], last_state[i] == DOWN ? true : false);
            if (motor[i] != STOP)
            {
                count_200ms[i]++;
                if (count_200ms[i] == 50)
                {
                    motor[i] = STOP;
                }
            }
            else
            {
                count_200ms[i] = 0;
            }
        }
        if (update)
        {
            csro_set_vibrator();
            csro_update_motor_4k4r_state();
        }
    }
    vTaskDelete(NULL);
}

static void motor_4k4r_key_task(void *args)
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
            }
            else
            {
                holdtime[i] = 0;
            }
        }
        if (holdtime[0] == 2)
        {
            motor_state state = motor[1];
            motor[1] = (state == STOP) ? UP : STOP;
        }
        else if (holdtime[2] == 2)
        {
            motor_state state = motor[1];
            motor[1] = (state == STOP) ? DOWN : STOP;
        }
        if (holdtime[1] == 2)
        {
            motor_state state = motor[2];
            motor[2] = (state == STOP) ? UP : STOP;
        }
        else if (holdtime[3] == 2)
        {
            motor_state state = motor[2];
            motor[2] = (state == STOP) ? DOWN : STOP;
        }
        vTaskDelay(20 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void csro_motor_4k4r_init(void)
{
    csro_aw9523b_init();
    xTaskCreate(motor_4k4r_relay_led_task, "nlight_4k4r_relay_led_task", 2048, NULL, configMAX_PRIORITIES - 8, NULL);
    xTaskCreate(motor_4k4r_key_task, "nlight_4k4r_key_task", 2048, NULL, configMAX_PRIORITIES - 6, NULL);
}
void csro_motor_4k4r_on_connect(esp_mqtt_event_handle_t event)
{
    sprintf(mqttinfo.sub_topic, "csro/%s/%s/set/#", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_subscribe(event->client, mqttinfo.sub_topic, 0);

    for (size_t i = 0; i < 2; i++)
    {
        char prefix[50], name[50], state[50], command[50];
        sprintf(mqttinfo.pub_topic, "csro/cover/%s_%s_%d/config", sysinfo.mac_str, sysinfo.dev_type, i);
        sprintf(prefix, "csro/%s/%s", sysinfo.mac_str, sysinfo.dev_type);
        sprintf(name, "%s_%d_%s", sysinfo.dev_type, i, sysinfo.mac_str);
        sprintf(state, "{{value_json.state[%d]}}", i);
        sprintf(command, "~/set/%d", i);

        cJSON *config_json = cJSON_CreateObject();
        cJSON_AddStringToObject(config_json, "~", prefix);
        cJSON_AddStringToObject(config_json, "name", name);
        cJSON_AddStringToObject(config_json, "cmd_t", command);
        cJSON_AddStringToObject(config_json, "stat_t", "~/state");
        cJSON_AddStringToObject(config_json, "avty_t", "~/available");
        cJSON_AddNumberToObject(config_json, "qos", 0);
        cJSON_AddStringToObject(config_json, "pl_open", "up");
        cJSON_AddStringToObject(config_json, "pl_stop", "stop");
        cJSON_AddStringToObject(config_json, "pl_cls", "down");
        cJSON_AddNumberToObject(config_json, "state_open", 1);
        cJSON_AddNumberToObject(config_json, "state_closed", 0);
        cJSON_AddStringToObject(config_json, "pl_avail", "online");
        cJSON_AddStringToObject(config_json, "pl_not_avail", "offline");
        cJSON_AddStringToObject(config_json, "value_template", state);
        cJSON_AddStringToObject(config_json, "opt", "false");

        char *out = cJSON_PrintUnformatted(config_json);
        strcpy(mqttinfo.content, out);
        free(out);
        cJSON_Delete(config_json);
        esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);
    }
    sprintf(mqttinfo.pub_topic, "csro/%s/%s/available", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, "online", 0, 0, 1);
    csro_update_motor_4k4r_state();
}
void csro_motor_4k4r_on_message(esp_mqtt_event_handle_t event)
{
    char topic[50];
    for (size_t i = 0; i < 2; i++)
    {
        sprintf(topic, "csro/%s/%s/set/%d", sysinfo.mac_str, sysinfo.dev_type, i);
        if (strncmp(topic, event->topic, event->topic_len) == 0)
        {
            if (strncmp("up", event->data, event->data_len) == 0)
            {
                motor[i] = UP;
            }
            else if (strncmp("stop", event->data, event->data_len) == 0)
            {
                motor[i] = STOP;
            }
            else if (strncmp("down", event->data, event->data_len) == 0)
            {
                motor[i] = DOWN;
            }
        }
    }
}

#endif