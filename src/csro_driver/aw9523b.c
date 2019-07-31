#include "aw9523b.h"

#define I2C_MASTER_SCL_IO 2
#define I2C_MASTER_SDA_IO 14
#define I2C_MASTER_NUM I2C_NUM_0

#ifdef NLIGHT_4K4R
uint8_t led_reg_addr[4] = {0x21, 0x2C, 0x20, 0x2D};
uint8_t relay_on_value[4] = {0x04, 0x08, 0x10, 0x20};
uint8_t relay_off_value[4] = {0xFB, 0xF7, 0xEF, 0xDF};
uint8_t vibrator_on_value = 0x01;
uint8_t vibrator_off_value = 0xFE;
#endif

#ifdef NLIGHT_6K4R
uint8_t led_reg_addr[6] = {0x20, 0x2D, 0x21, 0x2C, 0x22, 0x23};
uint8_t relay_on_value[4] = {0x04, 0x08, 0x10, 0x20};
uint8_t relay_off_value[4] = {0xFB, 0xF7, 0xEF, 0xDF};
uint8_t vibrator_on_value = 0x03;
uint8_t vibrator_off_value = 0xFC;
#endif

TimerHandle_t vibrator_timer;

static void i2c_master_aw9523b_write(uint8_t reg_addr, uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0xB6, 1);
    i2c_master_write_byte(cmd, reg_addr, 1);
    i2c_master_write_byte(cmd, value, 1);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 100 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
}

static uint8_t i2c_master_aw9523b_read(uint8_t reg_addr)
{
    uint8_t data = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0xB6, 1);
    i2c_master_write_byte(cmd, reg_addr, 1);
    i2c_master_stop(cmd);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0xB7, 1);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 100 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return data;
}

void vibrator_timer_callback(TimerHandle_t xTimer)
{
    uint8_t data = i2c_master_aw9523b_read(0x02);
    data = data & vibrator_off_value;
    i2c_master_aw9523b_write(0x02, data);
}

void csro_aw9523b_init(void)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = 0;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = 0;
    i2c_driver_install(I2C_MASTER_NUM, conf.mode);
    i2c_param_config(I2C_MASTER_NUM, &conf);

    i2c_master_aw9523b_write(0x12, 0xFF); //set P0.0-p0.7 gpio mode for vibrator and relays
    i2c_master_aw9523b_write(0x11, 0x10); //set P0.0-p0.7 gpio mode push-pull
    i2c_master_aw9523b_write(0x13, 0x00); //set P1.0-p1.7 led mode for keyboard leds
    i2c_master_aw9523b_write(0x02, 0x00); //set P0.0-p0.7 to low

    vibrator_timer = xTimerCreate("vibrator_timer", 130 / portTICK_RATE_MS, pdFALSE, (void *)0, vibrator_timer_callback);
    xTimerStart(vibrator_timer, portMAX_DELAY);
}

void csro_set_led(uint8_t led_num, uint8_t bright)
{
    i2c_master_aw9523b_write(led_reg_addr[led_num], bright);
}

void csro_set_relay(uint8_t relay_num, uint8_t state)
{
    uint8_t data = i2c_master_aw9523b_read(0x02);
    if (state == true)
    {
        data = data | relay_on_value[relay_num];
    }
    else
    {
        data = data & relay_off_value[relay_num];
    }
    i2c_master_aw9523b_write(0x02, data);
}

void csro_set_vibrator(void)
{
    uint8_t data = i2c_master_aw9523b_read(0x02);
    data = data | vibrator_on_value;
    i2c_master_aw9523b_write(0x02, data);
    xTimerReset(vibrator_timer, portMAX_DELAY);
}
