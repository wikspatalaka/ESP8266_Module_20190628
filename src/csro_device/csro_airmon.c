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

typedef struct
{
    uint32_t pm1_atm, pm2_atm, pm10_atm;
    float hcho, temp, humi;
} pms_data;

static QueueHandle_t uart0_queue;
static pms_data pms;

static void calculate_pms(uint8_t *data)
{
    static uint8_t count = 0;
    uint16_t sum = 0;
    for (size_t i = 0; i < 38; i++)
    {
        sum = sum + data[i];
    }
    if (data[38] == (sum >> 8) && data[39] == (sum & 0xFF))
    {
        uint8_t valid_count = (count < 5) ? count : 5;
        pms.pm1_atm = (data[10] * 256 + data[11] + pms.pm1_atm * valid_count) / (valid_count + 1);
        pms.pm2_atm = (data[12] * 256 + data[13] + pms.pm2_atm * valid_count) / (valid_count + 1);
        pms.pm10_atm = (data[14] * 256 + data[15] + pms.pm10_atm * valid_count) / (valid_count + 1);
        pms.hcho = ((float)(data[28] * 256 + data[29]) / 1000.0 + pms.hcho * valid_count) / (valid_count + 1);
        pms.temp = ((float)(data[30] * 256 + data[31]) / 10.0 + pms.temp * valid_count) / (valid_count + 1);
        pms.humi = ((float)(data[32] * 256 + data[33]) / 10.0 + pms.humi * valid_count) / (valid_count + 1);
        if (count < 5)
        {
            count++;
        }
        printf("pm1 = %d, pm2 = %d, pm10 = %d, hcho = %4.f, temp = %2.f, humi = %2.f\r\n", pms.pm1_atm, pms.pm2_atm, pms.pm10_atm, pms.hcho, pms.temp, pms.humi);
    }
}

static void uart_event_task(void *args)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(128);
    while (true)
    {
        if (xQueueReceive(uart0_queue, (void *)&event, portMAX_DELAY))
        {
            bzero(dtmp, 128);
            if (event.type == UART_DATA)
            {
                uart_read_bytes(UART_NUM_0, dtmp, event.size, portMAX_DELAY);
                calculate_pms(dtmp);
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
    static bool wifi_on = false;
    while (true)
    {
        vTaskDelay(500 / portTICK_RATE_MS);
        wifi_on = !wifi_on;
        if (wifi_on)
        {
            pwm_set_duty(0, 200 * 5);
            pwm_set_duty(1, 200 * 5);
            pwm_set_duty(2, 200 * 5);

            pwm_set_duty(3, 255 * 5);
            pwm_set_duty(4, 0 * 5);
            pwm_set_duty(5, 255 * 5);
        }
        else
        {
            pwm_set_duty(0, 0);
            pwm_set_duty(1, 0);
            pwm_set_duty(2, 0);

            pwm_set_duty(3, 0);
            pwm_set_duty(4, 0);
            pwm_set_duty(5, 0);
        }
        pwm_start();
    }
    vTaskDelete(NULL);
}

void csro_airmon_init(void)
{
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, configMAX_PRIORITIES - 8, NULL);
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 512 * 2, 512 * 2, 100, &uart0_queue);

    const uint32_t pin_num[7] = {WIFI_LED_R, WIFI_LED_G, WIFI_LED_B, AQI_LED_R, AQI_LED_G, AQI_LED_B, FAN_PIN};
    uint32_t duties[7] = {0, 0, 0, 0, 0, 0, (int)(0.6 * 2600)};
    int16_t phase[7] = {0, 0, 0, 0, 0, 0, 0};

    pwm_init(2600, duties, 7, pin_num);
    pwm_set_channel_invert(0x3F);
    pwm_set_phases(phase);
    pwm_start();
    xTaskCreate(led_task, "led_task", 2048, NULL, configMAX_PRIORITIES - 9, NULL);
}

void csro_airmon_on_connect(esp_mqtt_event_handle_t event)
{
    char *classlist[6] = {"temperature", "humidity", "pressure", "pressure", "pressure", "pressure"};
    char *itemlist[6] = {"temperature", "humidity", "hcho", "pm1dot0", "pm2dot5", "pm10"};
    char *unitlist[6] = {"°C", "%%", "mg/m^3", "ug/m^3", "ug/m^3", "ug/m^3"};
    char *iconlist[6] = {"mdi:thermometer-lines", "mdi:water-percent", "mdi:alien", "mdi:blur", "mdi:blur", "mdi:blur"};
    uint8_t roundlist[6] = {1, 1, 3, 0, 0, 0};
}

void csro_airmon_on_message(esp_mqtt_event_handle_t event)
{
}

#endif