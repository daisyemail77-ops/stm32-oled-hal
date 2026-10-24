/**
 * aht20.c - AHT20 温湿度传感器驱动（HAL I2C，阻塞轮询）
 *
 * 协议要点（AHT20 数据手册）：
 *   7 位地址 0x38 → HAL 用 8 位地址：写 0x70 / 读 0x71
 *   上电后等 >40ms → 读状态字 → 若 bit3(校准完成) 不为 1，发 0xBE,0x08,0x00，等 10ms
 *   触发测量 0xAC,0x33,0x00 → 等 75~80ms → 读 6 字节：
 *     data[0]      bit7 = 忙标志（1 = 还在测量）
 *     data[1..3]   湿度 20 位（data[1] 高 8 位 … data[3] 高 4 位）
 *     data[3]低4位 + data[4..5]  温度 20 位
 *   换算：湿度(%) = raw_h / 2^20 × 100
 *         温度(℃) = raw_t / 2^20 × 200 - 50
 *
 * 定点换算（结果 ×10，全用 32 位整数，无需浮点）：
 *   湿度×10 = raw_h * 1000 / 2^20
 *   温度×10 = raw_t * 2000 / 2^20 - 500
 *   ⚠ 温度这一项分母相同但分子是 2000，写成 1000 会得到只有真实值一半的温度，
 *     例如 25.0℃ 会显示成 -12.5℃。
 *   注：raw_t 最大 1048575，×2000 = 2097150000 < 2^32，不会溢出。
 */
#include "aht20.h"

#define AHT20_ADDR      0x38                    /* 7 位地址 */
#define AHT20_ADDR_W    (AHT20_ADDR << 1)       /* 0x70 写 */
#define AHT20_ADDR_R    ((AHT20_ADDR << 1) | 1) /* 0x71 读 */

#define AHT20_CMD_INIT  0xBE
#define AHT20_CMD_TRIG  0xAC
#define AHT20_WAIT_MS   80U                     /* 一次测量约 75~80ms */
#define AHT20_TIMEOUT   50U                     /* 单次 I2C 事务超时(ms) */
#define AHT20_TRIES     3U                      /* 初始化/读数据重试次数 */

/* 读状态字，成功返回 0 */
static uint8_t aht20_read_status(I2C_HandleTypeDef *hi2c, uint8_t *status)
{
    if (HAL_I2C_Master_Receive(hi2c, AHT20_ADDR_R, status, 1, AHT20_TIMEOUT) != HAL_OK) {
        return 1;
    }
    return 0;
}

/* 发初始化命令 0xBE 0x08 0x00，成功返回 0 */
static uint8_t aht20_send_init(I2C_HandleTypeDef *hi2c)
{
    uint8_t cmd[3] = { AHT20_CMD_INIT, 0x08, 0x00 };

    if (HAL_I2C_Master_Transmit(hi2c, AHT20_ADDR_W, cmd, 3, AHT20_TIMEOUT) != HAL_OK) {
        return 1;
    }
    HAL_Delay(10);
    return 0;
}

uint8_t AHT20_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t status = 0;

    HAL_Delay(40);                              /* 上电稳定（手册要求 >40ms） */

    for (uint8_t i = 0; i < AHT20_TRIES; i++) {
        /* 已经校准好（bit3=1）且不忙（bit7=0）就不用再发初始化命令 */
        if (aht20_read_status(hi2c, &status) == 0) {
            if ((status & 0x18) == 0x18) {
                return 0;
            }
        }
        if (aht20_send_init(hi2c) != 0) {
            return 1;                           /* 写失败：器件没应答 */
        }
    }

    /* 三次之后再看一眼状态字 */
    if (aht20_read_status(hi2c, &status) != 0) {
        return 2;                               /* 读失败 */
    }
    if ((status & 0x80) != 0) {
        return 3;                               /* 一直忙 */
    }
    if ((status & 0x08) == 0) {
        return 4;                               /* 校准位没有置 1 */
    }
    return 0;
}

uint8_t AHT20_ReadData(I2C_HandleTypeDef *hi2c, int16_t *temp_x10, uint16_t *humi_x10)
{
    uint8_t cmd[3]  = { AHT20_CMD_TRIG, 0x33, 0x00 };
    uint8_t data[6] = { 0 };
    uint32_t raw_h, raw_t;

    if (HAL_I2C_Master_Transmit(hi2c, AHT20_ADDR_W, cmd, 3, AHT20_TIMEOUT) != HAL_OK) {
        return 1;                               /* 触发命令写失败 */
    }
    HAL_Delay(AHT20_WAIT_MS);

    /* 读完再判忙，偶尔还会是 1，重试几次 */
    for (uint8_t i = 0; i < AHT20_TRIES; i++) {
        if (HAL_I2C_Master_Receive(hi2c, AHT20_ADDR_R, data, 6, AHT20_TIMEOUT) != HAL_OK) {
            return 2;                           /* 读失败 */
        }
        if ((data[0] & 0x80) == 0) {
            break;                              /* 不忙，数据有效 */
        }
        HAL_Delay(10);
        if (i == AHT20_TRIES - 1) {
            return 3;                           /* 一直忙 */
        }
    }

    /* 湿度 20 位：data[1] 高 8 位 + data[2] + data[3] 高 4 位 */
    raw_h = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((uint32_t)data[3] >> 4);
    /* 温度 20 位：data[3] 低 4 位 + data[4] + data[5] */
    raw_t = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5];

    /* +524288 相当于四舍五入（2^20 / 2），比直接截断更准 */
    *humi_x10 = (uint16_t)(((raw_h * 1000UL) + 524288UL) / 1048576UL);              /* 0~1000 */
    *temp_x10 = (int16_t)(((raw_t * 2000UL) + 524288UL) / 1048576UL) - 500;          /* -500~1500 */

    if (*humi_x10 > 1000) {
        *humi_x10 = 1000;                       /* 超量程时夹住，别显示 100.3% */
    }
    return 0;
}

I2C_HandleTypeDef *AHT20_Begin(I2C_HandleTypeDef *bus_primary, I2C_HandleTypeDef *bus_alt)
{
    if (bus_primary != NULL &&
        HAL_I2C_IsDeviceReady(bus_primary, AHT20_ADDR_W, 2, 20) == HAL_OK &&
        AHT20_Init(bus_primary) == 0) {
        return bus_primary;
    }
    if (bus_alt != NULL && bus_alt != bus_primary &&
        HAL_I2C_IsDeviceReady(bus_alt, AHT20_ADDR_W, 2, 20) == HAL_OK &&
        AHT20_Init(bus_alt) == 0) {
        return bus_alt;
    }
    return NULL;
}
