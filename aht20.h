/**
 * aht20.h - AHT20 温湿度传感器驱动（HAL I2C）
 *
 * 地址：7 位地址 0x38 → HAL 写地址 0x70 / 读地址 0x71
 * 总线：默认挂 I2C2（PB10=SCL, PB11=SDA）；
 *       也可以和 OLED 一起挂在 I2C1（PB8/PB9）上，地址不冲突。
 */
#ifndef AHT20_H
#define AHT20_H

#include "stm32f1xx_hal.h"

/* 初始化：上电延时 40ms → 发 0xBE,0x08,0x00 → 检查状态字 bit3 校准位（最多重试 3 次）
 * 返回 0 成功；非 0 为失败码（1 写失败 / 2 读失败 / 3 一直忙 / 4 校准未完成） */
uint8_t AHT20_Init(I2C_HandleTypeDef *hi2c);

/* 触发一次测量（0xAC,0x33,0x00），等待 80ms 后读 6 字节并换算
 * 返回 0 成功；非 0 为失败码（1 写失败 / 2 读失败 / 3 一直忙）
 * 成功时：*temp_x10 = 温度×10（如 25.3℃ → 253）；*humi_x10 = 湿度×10（如 62.5% → 625） */
uint8_t AHT20_ReadData(I2C_HandleTypeDef *hi2c, int16_t *temp_x10, uint16_t *humi_x10);

/* 在两条 I2C 总线上探测 AHT20 并完成初始化，返回可用的总线句柄；都找不到返回 NULL。
 * bus_primary 一般传 &hi2c2（PB10/PB11），bus_alt 传 &hi2c1（与 OLED 共用）。 */
I2C_HandleTypeDef *AHT20_Begin(I2C_HandleTypeDef *bus_primary, I2C_HandleTypeDef *bus_alt);

#endif
