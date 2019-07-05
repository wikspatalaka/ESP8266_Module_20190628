#include "aw9523b.h"

#ifdef NLIGHT

#define I2C_MASTER_SCL_IO 2
#define I2C_MASTER_SDA_IO 14
#define I2C_MASTER_NUM I2C_NUM_0

#if NLIGHT == 3
uint8_t LED_REG_ADDR[] = {0x22, 0x20, 0x2C, 0x21, 0x23, 0x2D};
#endif

void i2c_master_aw9523b_write(uint8_t reg_addr, uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0xB6, 1);
    i2c_master_write_byte(cmd, reg_addr, 1);
    i2c_master_write_byte(cmd, value, 1);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
}

uint8_t i2c_master_aw9523b_read(uint8_t reg_addr)
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
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return data;
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
    i2c_master_aw9523b_write(0x11, 0x10); //set P0.0-p0.7 gpio mode for vibrator and relays
    i2c_master_aw9523b_write(0x13, 0x00); //set P1.0-p1.7 led mode for keyboard leds
}

void csro_set_led(uint8_t led_num, uint8_t bright)
{
    i2c_master_aw9523b_write(LED_REG_ADDR[led_num - 1], bright);
}

void csro_set_relay(uint8_t relay_num, uint8_t state)
{
    uint8_t data = i2c_master_aw9523b_read(0x02);
    if (state == true)
    {
        data = data | (0x01 << (1 + relay_num));
    }
    else
    {
        switch (relay_num)
        {
        case 1:
            data = data & 0xFB;
            break;
        case 2:
            data = data & 0xF7;
            break;
        case 3:
            data = data & 0xEF;
            break;
        case 4:
            data = data & 0xDF;
            break;
        default:
            break;
        }
    }
    i2c_master_aw9523b_write(0x02, data);
}

void csro_set_vibrator(uint8_t period_ms)
{
}

#endif