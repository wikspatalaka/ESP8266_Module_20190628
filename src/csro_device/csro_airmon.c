#include "csro_devices.h"
#include "uart.h"
#include "pwm.h"

#ifdef AIR_MONITOR

#define WIFI_LED_R 5
#define WIFI_LED_G 4
#define WIFI_LED_B 10
#define AQI_LED_R 12
#define AQI_LED_G 14
#define AQI_LED_B 13
#define FAN_PIN 15
#define BTN_PIN 0
#define AVE_COUNT 5

#define BLUE 0, 0, 255
#define GREEN 0, 255, 0
#define GREENYELLOW 173, 255, 47
#define YELLOW 255, 255, 0
#define ORANGE 255, 102, 0
#define RED 255, 0, 0

// typedef enum
// {
//     BLUE = 0,    //red=0,   green=0; 	blue=255;
//     GREEN,       //red=0,   green=255; 	blue=0;
//     GREENYELLOW, //red=173, green=255; 	blue=47;
//     YELLOW,      //red=255, green=255; 	blue=0;
//     ORANGE,      //red=255, green=102; 	blue=0;
//     RED,         //red=255, green=0; 	blue=0;
// } rgb_color;

typedef struct
{
    uint32_t pm1_atm[AVE_COUNT + 1];
    uint32_t pm2_atm[AVE_COUNT + 1];
    uint32_t pm10_atm[AVE_COUNT + 1];
    float hcho[AVE_COUNT + 1];
    float temp[AVE_COUNT + 1];
    float humi[AVE_COUNT + 1];
} pms_data;

static char *itemlist[6] = {"temp", "humi", "hcho", "pm1", "pm2d5", "pm10"};
static char *unitlist[6] = {"°C", "\%", "mg/m^3", "ug/m^3", "ug/m^3", "ug/m^3"};
static char *iconlist[6] = {"mdi:thermometer-lines", "mdi:water-percent", "mdi:alien", "mdi:blur", "mdi:blur", "mdi:blur"};

static uint8_t color[6][3] = {{BLUE}, {GREEN}, {GREENYELLOW}, {YELLOW}, {ORANGE}, {RED}};
static uint8_t roundlist[6] = {1, 1, 3, 0, 0, 0};
static uint8_t pm_array[5] = {35, 75, 115, 150, 250};
static float hchi_array[5] = {0.05, 0.1, 0.2, 0.5, 0.8};

static QueueHandle_t uart0_queue;
static xSemaphoreHandle pub_sem;
static pms_data pms;
static uint8_t aqi_index = 0;

static void receive_sensor_values_task(void *arg)
{
    while (true)
    {
        if (xSemaphoreTake(pub_sem, portMAX_DELAY) == pdTRUE)
        {
            pms.pm1_atm[AVE_COUNT] = 0;
            pms.pm2_atm[AVE_COUNT] = 0;
            pms.pm10_atm[AVE_COUNT] = 0;
            pms.hcho[AVE_COUNT] = 0;
            pms.temp[AVE_COUNT] = 0;
            pms.humi[AVE_COUNT] = 0;
            for (size_t i = 0; i < AVE_COUNT; i++)
            {
                pms.pm1_atm[AVE_COUNT] = pms.pm1_atm[AVE_COUNT] + pms.pm1_atm[i];
                pms.pm2_atm[AVE_COUNT] = pms.pm2_atm[AVE_COUNT] + pms.pm2_atm[i];
                pms.pm10_atm[AVE_COUNT] = pms.pm10_atm[AVE_COUNT] + pms.pm10_atm[i];
                pms.hcho[AVE_COUNT] = pms.hcho[AVE_COUNT] + pms.hcho[i];
                pms.temp[AVE_COUNT] = pms.temp[AVE_COUNT] + pms.temp[i];
                pms.humi[AVE_COUNT] = pms.humi[AVE_COUNT] + pms.humi[i];
            }
            pms.pm1_atm[AVE_COUNT] = pms.pm1_atm[AVE_COUNT] / AVE_COUNT;
            pms.pm2_atm[AVE_COUNT] = pms.pm2_atm[AVE_COUNT] / AVE_COUNT;
            pms.pm10_atm[AVE_COUNT] = pms.pm10_atm[AVE_COUNT] / AVE_COUNT;
            pms.hcho[AVE_COUNT] = pms.hcho[AVE_COUNT] / AVE_COUNT;
            pms.temp[AVE_COUNT] = pms.temp[AVE_COUNT] / AVE_COUNT;
            pms.humi[AVE_COUNT] = pms.humi[AVE_COUNT] / AVE_COUNT;

            uint8_t aqi_temp = 0;
            for (size_t i = 0; i < 5; i++)
            {
                if ((pms.pm2_atm[AVE_COUNT] > pm_array[i]) || (pms.hcho[AVE_COUNT] > hchi_array[i]))
                {
                    aqi_temp = i + 1;
                }
                else
                {
                    aqi_index = aqi_temp;
                    break;
                }
            }

            cJSON *sensor_json = cJSON_CreateObject();
            cJSON_AddNumberToObject(sensor_json, "pm1", pms.pm1_atm[AVE_COUNT]);
            cJSON_AddNumberToObject(sensor_json, "pm2d5", pms.pm2_atm[AVE_COUNT]);
            cJSON_AddNumberToObject(sensor_json, "pm10", pms.pm10_atm[AVE_COUNT]);
            cJSON_AddNumberToObject(sensor_json, "temp", pms.temp[AVE_COUNT]);
            cJSON_AddNumberToObject(sensor_json, "humi", pms.humi[AVE_COUNT]);
            cJSON_AddNumberToObject(sensor_json, "hcho", pms.hcho[AVE_COUNT]);
            cJSON_AddNumberToObject(sensor_json, "run", sysinfo.time_run);
            char *out = cJSON_PrintUnformatted(sensor_json);
            strcpy(mqttinfo.content, out);
            free(out);
            cJSON_Delete(sensor_json);
            sprintf(mqttinfo.pub_topic, "csro/%s/%s/state", sysinfo.mac_str, sysinfo.dev_type);
            if (esp_mqtt_client_is_available(mqttclient))
            {
                esp_mqtt_client_publish(mqttclient, mqttinfo.pub_topic, mqttinfo.content, 0, 0, 1);
            }
        }
    }
    vTaskDelete(NULL);
}

static void uart_event_task(void *args)
{
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 512 * 2, 512 * 2, 100, &uart0_queue);

    uart_event_t event;
    static uint8_t msg_count = 0;
    uint8_t *dtmp = (uint8_t *)malloc(128);
    while (true)
    {
        if (xQueueReceive(uart0_queue, (void *)&event, portMAX_DELAY))
        {
            bzero(dtmp, 128);
            if (event.type == UART_DATA)
            {
                uart_read_bytes(UART_NUM_0, dtmp, event.size, portMAX_DELAY);
                uint16_t sum = 0;
                for (size_t i = 0; i < 38; i++)
                {
                    sum = sum + dtmp[i];
                }
                if (dtmp[38] == (sum >> 8) && dtmp[39] == (sum & 0xFF))
                {
                    pms.pm1_atm[msg_count] = dtmp[10] * 256 + dtmp[11];
                    pms.pm2_atm[msg_count] = dtmp[12] * 256 + dtmp[13];
                    pms.pm10_atm[msg_count] = dtmp[14] * 256 + dtmp[15];
                    pms.hcho[msg_count] = (float)(dtmp[28] * 256 + dtmp[29]) / 1000.0;
                    pms.temp[msg_count] = (float)(dtmp[30] * 256 + dtmp[31]) / 10.0;
                    pms.humi[msg_count] = (float)(dtmp[32] * 256 + dtmp[33]) / 10.0;
                    msg_count = (msg_count + 1) % AVE_COUNT;
                    if (msg_count == 0)
                    {
                        xSemaphoreGive(pub_sem);
                    }
                }
            }
            else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL)
            {
                uart_flush_input(UART_NUM_0);
                xQueueReset(uart0_queue);
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

static void led_task(void *arg)
{
    const uint32_t pin_num[7] = {WIFI_LED_R, WIFI_LED_G, WIFI_LED_B, AQI_LED_R, AQI_LED_G, AQI_LED_B, FAN_PIN};
    uint32_t duties[7] = {0, 0, 0, 0, 0, 0, (int)(0.6 * 2600)};
    int16_t phase[7] = {0, 0, 0, 0, 0, 0, 0};
    pwm_init(2600, duties, 7, pin_num);
    pwm_set_channel_invert(0x3F);
    pwm_set_phases(phase);
    pwm_start();
    bool flash = false;
    while (true)
    {
        flash = !flash;
        if (flash)
        {
            if (esp_mqtt_client_is_available(mqttclient))
            {
                pwm_set_duty(0, color[0][0] * 10);
                pwm_set_duty(1, color[0][1] * 10);
                pwm_set_duty(2, color[0][2] * 10);
            }
            else
            {
                pwm_set_duty(0, color[5][0] * 10);
                pwm_set_duty(1, color[5][1] * 10);
                pwm_set_duty(2, color[5][2] * 10);
            }
        }
        else
        {
            pwm_set_duty(0, 0);
            pwm_set_duty(1, 0);
            pwm_set_duty(2, 0);
        }
        pwm_set_duty(3, color[aqi_index][0] * 10);
        pwm_set_duty(4, color[aqi_index][1] * 10);
        pwm_set_duty(5, color[aqi_index][2] * 10);
        pwm_start();
        vTaskDelay(500 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void csro_airmon_init(void)
{
    pub_sem = xSemaphoreCreateBinary();
    xTaskCreate(receive_sensor_values_task, "receive_sensor_values_task", 2048, NULL, configMAX_PRIORITIES - 7, NULL);
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, configMAX_PRIORITIES - 8, NULL);
    xTaskCreate(led_task, "led_task", 2048, NULL, configMAX_PRIORITIES - 9, NULL);
}

void csro_airmon_on_connect(esp_mqtt_event_handle_t event)
{
    for (size_t i = 0; i < 6; i++)
    {
        char prefix[50], name[50], value_template[50];
        sprintf(mqttinfo.pub_topic, "csro/sensor/%s_%s_%s/config", sysinfo.mac_str, sysinfo.dev_type, itemlist[i]);
        sprintf(prefix, "csro/%s/%s", sysinfo.mac_str, sysinfo.dev_type);
        sprintf(name, "%s_%s_%s", sysinfo.dev_type, itemlist[i], sysinfo.mac_str);
        sprintf(value_template, "{{value_json.%s | round(%d)}}", itemlist[i], roundlist[i]);
        cJSON *config_json = cJSON_CreateObject();
        cJSON_AddStringToObject(config_json, "~", prefix);
        cJSON_AddStringToObject(config_json, "name", name);
        cJSON_AddStringToObject(config_json, "unit_of_meas", unitlist[i]);
        cJSON_AddStringToObject(config_json, "avty_t", "~/available");
        cJSON_AddStringToObject(config_json, "pl_avail", "online");
        cJSON_AddStringToObject(config_json, "pl_not_avail", "offline");
        cJSON_AddStringToObject(config_json, "stat_t", "~/state");
        cJSON_AddStringToObject(config_json, "icon", iconlist[i]);
        cJSON_AddStringToObject(config_json, "val_tpl", value_template);
        cJSON_AddNumberToObject(config_json, "qos", 1);
        char *out = cJSON_PrintUnformatted(config_json);
        strcpy(mqttinfo.content, out);
        free(out);
        cJSON_Delete(config_json);
        esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, mqttinfo.content, 0, 1, 1);
    }
    sprintf(mqttinfo.pub_topic, "csro/%s/%s/available", sysinfo.mac_str, sysinfo.dev_type);
    esp_mqtt_client_publish(event->client, mqttinfo.pub_topic, "online", 0, 1, 1);
}

void csro_airmon_on_message(esp_mqtt_event_handle_t event)
{
}

#endif