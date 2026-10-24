/**
 * 实验 6：OLED 显示 + U8g2 移植 + AHT20 + 汉字点阵（STM32F103C8T6 / HAL / Keil AC6）
 *
 * ── 程序框架 ───────────────────────────────────────────────────────────
 *   本文件按 STM32CubeMX 生成代码的结构组织（CubeMX 配置见实验报告 5.1 节）：
 *     HAL_Init() → SystemClock_Config() → MX_xxx_Init() → 外设初始化回调 → 主循环
 *   外设：I2C1(OLED) / I2C2(AHT20) / USART1(调试串口, 可测波形)
 *
 * ── 硬件连接 ───────────────────────────────────────────────────────────
 *   OLED  (SSD1306, 7 位地址 0x3C) : VCC-3.3V  GND-GND  SCL-PB8   SDA-PB9   (I2C1 重映射)
 *   AHT20 (温湿度,  7 位地址 0x38) : VIN-3.3V   GND-GND  SCL-PB10  SDA-PB11  (I2C2)
 *   AHT20 也可以与 OLED 挂在同一条 I2C1(PB8/PB9) 总线上，启动时自动探测
 *   串口   USART1 115200 8N1      : PA9=TX  PA10=RX（接 USB-TTL，可选）
 *
 * ── 循环显示的 6 个画面 ────────────────────────────────────────────────
 *   ① U8g2 图形 demo：边框 + 标题 + 弹跳小球 + 进度条（直线/圆/矩形/位图 API）
 *   ② 学号 + 姓名：代思雨 632403091001
 *   ③ AHT20 温湿度：温度 xx.x℃ / 湿度 xx.x%RH
 *   ④ 左右滚动：长字符串 "Hello，欢迎来到重庆交通大学物联网205实训室！"
 *   ⑤ 上下滚动：垂直方向滑动 "重庆交通大学"
 *   ⑥ 头像动画：64x64 像素头像，眨眼 + 上下轻微浮动，右侧竖排姓名
 *
 * ── 移植要点 ───────────────────────────────────────────────────────────
 *   1. 汉字/ASCII 点阵为自建字库（font_hz16.c / font_asc8x16.c），
 *      格式：逐行存放，每字节 MSB 在左。u8g2_DrawXBM 按 LSB 在左解析，
 *      直接用会把字形左右镜像，所以这里用 draw_bitmap() 自己逐像素画。
 *   2. 刷屏用 u8g2 全缓冲 f 模式：ClearBuffer → 绘图 → SendBuffer，
 *      整屏 1024 字节一次写入显存（硬件自动递增寻址），约 25ms/帧。
 *   3. u8g2 的 I2C 底层由 ssd13xx_fast_i2c CAD 层驱动，一次事务 =
 *      START + (0x00|0x40) + 数据 + STOP，故 byte 回调采用“缓冲式”。
 */
#include "stm32f1xx_hal.h"
#include "u8g2.h"
#include "font_hz16.h"
#include "font_asc8x16.h"
#include "aht20.h"
#include "avatar.h"
#include <string.h>

/* =============== 全局句柄 =============== */
I2C_HandleTypeDef hi2c1;   /* OLED  (I2C1, PB8=SCL / PB9=SDA, 重映射) */
I2C_HandleTypeDef hi2c2;   /* AHT20 (I2C2, PB10=SCL / PB11=SDA) */
UART_HandleTypeDef huart1; /* 调试串口 (USART1, PA9=TX / PA10=RX) */
u8g2_t u8g2;

#define OLED_W          128
#define OLED_H          64
#define I2C_TIMEOUT_MS  25      /* 一次 I2C 事务超时（25 字节 @400k 约 0.6ms） */
#define DEMO_MS         6000    /* ① 图形 demo 持续时间 */

/* =============== 函数声明 =============== */
void Error_Handler(void);
static void SystemClock_Config(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART1_UART_Init(void);

/* =============== 系统时钟：HSE 8MHz × PLL9 = 72MHz =============== */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        /* 板上没有 8MHz 晶振时不要死等：退回内部 HSI，8MHz/2×16 = 64MHz */
        RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
        RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
        RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI_DIV2;
        RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLL_MUL16;
        if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
            Error_Handler();
        }
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource    = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider   = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider  = RCC_HCLK_DIV2;   /* I2C/USART2 挂在 APB1 */
    RCC_ClkInitStruct.APB2CLKDivider  = RCC_HCLK_DIV1;   /* USART1 挂在 APB2  */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/* =============== 外设初始化 =============== */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 400000;            /* 400kHz 快速模式 */
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_16_9;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_I2C2_Init(void)
{
    hi2c2.Instance             = I2C2;
    hi2c2.Init.ClockSpeed      = 400000;
    hi2c2.Init.DutyCycle       = I2C_DUTYCYCLE_16_9;
    hi2c2.Init.OwnAddress1     = 0;
    hi2c2.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2     = 0;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

/* =============== MSP：GPIO 复用 + 时钟 =============== */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_AFIO_CLK_ENABLE();                 /* 重映射必须先开 AFIO 时钟 */
        __HAL_AFIO_REMAP_I2C1_ENABLE();              /* I2C1 重映射到 PB8/PB9 */
        GPIO_InitStruct.Pin   = GPIO_PIN_8 | GPIO_PIN_9;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_OD;     /* 复用开漏（模块板载上拉） */
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    } else if (hi2c->Instance == I2C2) {
        __HAL_RCC_I2C2_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitStruct.Pin   = GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitStruct.Pin   = GPIO_PIN_9;          /* PA9 = TX */
        GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin  = GPIO_PIN_10;          /* PA10 = RX */
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

/* =============== U8g2 底层回调：缓冲式 I2C 字节发送 ===============
 * CAD 层一次事务最长：控制字节(1) + 24 字节数据 = 25 字节；
 * 命令事务：0x00 + 命令 + 参数（SSD1306 最多 3 个参数）。64 字节足够。
 */
#define U8X8_I2C_TX_MAX   64
static uint8_t u8x8_txbuf[U8X8_I2C_TX_MAX];
static uint8_t u8x8_txlen;

uint8_t u8x8_byte_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    uint8_t *p = (uint8_t *)arg_ptr;

    switch (msg) {
    case U8X8_MSG_BYTE_START_TRANSFER:      /* 事务开始：清缓冲 */
        u8x8_txlen = 0;
        break;

    case U8X8_MSG_BYTE_SEND:                /* 追加数据（控制字节 0x00/0x40 也走这里） */
        while (arg_int-- > 0) {
            if (u8x8_txlen < U8X8_I2C_TX_MAX) {
                u8x8_txbuf[u8x8_txlen++] = *p;
            }
            p++;
        }
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:        /* 事务结束：一次发完 */
        if (u8x8_txlen > 0) {
            HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)u8x8->i2c_address,
                                    u8x8_txbuf, u8x8_txlen, I2C_TIMEOUT_MS);
        }
        u8x8_txlen = 0;
        break;

    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_SET_DC:
        break;

    default:
        return 0;
    }
    return 1;
}

uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    (void)arg_ptr;

    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        break;
    case U8X8_MSG_DELAY_MILLI:
        HAL_Delay(arg_int);
        break;
    case U8X8_MSG_DELAY_10MICRO:
    case U8X8_MSG_DELAY_100NANO:
        break;
    default:
        return 0;
    }
    return 1;
}

/* =============== 点阵/位图绘制 ===============
 * 字模格式：逐行存放，每行 (w+7)/8 字节，每字节 MSB 是左边的像素。
 * 不能直接用 u8g2_DrawXBM：它按 XBM 规范“LSB 在左”读同一个字节，
 * 会把每个 8 像素块左右翻转（汉字被劈成两半各自镜像）。
 */
static void draw_bitmap(u8g2_t *u, int x, int y, uint8_t w, uint8_t h, const uint8_t *bm)
{
    const uint8_t bpr = (uint8_t)((w + 7) >> 3);   /* 每行字节数：8→1，16→2，64→8 */
    int c0, c1, r0, r1;

    if (bm == 0) {
        return;                                     /* 字库里没有这个字 */
    }

    c0 = (x < 0) ? -x : 0;                          /* 左侧被裁掉的列 */
    c1 = (x + w > OLED_W) ? (OLED_W - x) : w;       /* 右侧可见的列 */
    r0 = (y < 0) ? -y : 0;                          /* 上边被裁掉的行 */
    r1 = (y + h > OLED_H) ? (OLED_H - y) : h;       /* 下边可见的行 */
    if (c0 >= c1 || r0 >= r1) {
        return;                                     /* 整个图形都在屏外 */
    }

    for (int row = r0; row < r1; row++) {
        const uint8_t *line = bm + (uint16_t)row * bpr;

        for (int col = c0; col < c1; col++) {
            if (line[col >> 3] & (uint8_t)(0x80 >> (col & 7))) {
                u8g2_DrawPixel(u, (u8g2_uint_t)(x + col), (u8g2_uint_t)(y + row));
            }
        }
    }
}

/* 16 行高的字形（8x16 ASCII / 16x16 汉字） */
static void draw_glyph(u8g2_t *u, int x, int y, uint8_t w, const uint8_t *bm)
{
    draw_bitmap(u, x, y, w, 16, bm);
}

/* UTF-8 字符占几个字节（本程序只会遇到 1 和 3） */
static uint8_t utf8_len(const char *s)
{
    uint8_t c = (uint8_t)s[0];

    if (c < 0x80)            return 1;
    if ((c & 0xE0) == 0xC0)  return 2;
    if ((c & 0xF0) == 0xE0)  return 3;
    if ((c & 0xF8) == 0xF0)  return 4;
    return 1;
}

/* 字符串像素宽度：ASCII 8px，汉字/全角 16px */
static int str_width(const char *s)
{
    int w = 0;

    while (*s) {
        uint8_t n = utf8_len(s);
        w += (n == 1) ? 8 : 16;
        s += n;
    }
    return w;
}

/* 混合字符串绘制：ASCII 8x16 / 汉字 16x16；x、y 可以为负（滚动时用） */
static void draw_str(u8g2_t *u, int x, int y, const char *s)
{
    while (*s) {
        uint8_t n = utf8_len(s);

        if (n == 1) {
            uint8_t ch = (uint8_t)*s;
            if (ch >= 32 && ch <= 126) {
                draw_glyph(u, x, y, 8, font_asc8x16[ch - 32]);
            }
            x += 8;
            s += 1;
        } else {
            draw_glyph(u, x, y, 16, font16_find(s));
            x += 16;
            s += n;
        }
    }
}

/* 水平居中绘制 */
static void draw_str_center(u8g2_t *u, int y, const char *s)
{
    draw_str(u, (OLED_W - str_width(s)) / 2, y, s);
}

/* 把 v_x10 格式化成 "25.3" / "-5.0" / "100.0"，返回写入的字符数 */
static uint8_t fmt_x10(char *buf, int16_t v_x10)
{
    uint8_t  i = 0;
    uint16_t v;
    uint16_t ip;

    if (v_x10 < 0) {
        buf[i++] = '-';
        v = (uint16_t)(-v_x10);
    } else {
        v = (uint16_t)v_x10;
    }

    ip = (uint16_t)(v / 10);                /* 整数部分：0~150 */
    if (ip >= 100) {
        buf[i++] = (char)('0' + (ip / 100) % 10);
    }
    if (ip >= 10) {
        buf[i++] = (char)('0' + (ip / 10) % 10);
    }
    buf[i++] = (char)('0' + ip % 10);
    buf[i++] = '.';
    buf[i++] = (char)('0' + v % 10);
    buf[i]   = '\0';
    return i;
}

/* =============== 调试串口（任务2 可在这里测串口波形） =============== */
static void uart_puts(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 100);
}

static void uart_log_aht(uint8_t ok, int16_t t_x10, uint16_t h_x10)
{
    char v[8];

    uart_puts("[AHT20] ");
    if (ok) {
        fmt_x10(v, t_x10);
        uart_puts("T=");
        uart_puts(v);
        uart_puts("C  H=");
        fmt_x10(v, (int16_t)h_x10);
        uart_puts(v);
        uart_puts("%RH\r\n");
    } else {
        uart_puts("read fail\r\n");
    }
}

/* =============== 画面 ①：U8g2 图形 demo ===============
 * 用到 u8g2 的 DrawFrame / DrawLine / DrawDisc / DrawBox + 自建字库文字，
 * 小球在框内弹跳，底部进度条同时演示填充矩形。
 */
static void demo_graphics(void)
{
    int ball_x = 40, ball_y = 36, vx = 3, vy = 2;
    uint32_t t0 = HAL_GetTick();
    uint32_t t;

    do {
        t = HAL_GetTick() - t0;

        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawFrame(&u8g2, 0, 0, OLED_W, OLED_H);          /* 外框 */
        draw_str(&u8g2, 4, 2, "U8G2 DEMO");                   /* 标题 */
        u8g2_DrawLine(&u8g2, 3, 20, 124, 20);                 /* 分隔线 */

        u8g2_DrawDisc(&u8g2, (u8g2_uint_t)ball_x, (u8g2_uint_t)ball_y, 4, U8G2_DRAW_ALL);

        /* 进度条：外框 + 按时间填充 */
        u8g2_DrawFrame(&u8g2, 6, 54, 116, 6);
        if (t > 0) {
            u8g2_DrawBox(&u8g2, 7, 55, (u8g2_uint_t)(114UL * t / DEMO_MS), 4);
        }

        u8g2_SendBuffer(&u8g2);

        /* 撞墙反弹：可活动区域 x 7..120、y 26..49 */
        ball_x += vx;
        ball_y += vy;
        if (ball_x < 7 || ball_x > 120) { vx = -vx; ball_x += vx * 2; }
        if (ball_y < 26 || ball_y > 49) { vy = -vy; ball_y += vy * 2; }

        HAL_Delay(30);
    } while (t < DEMO_MS);
}

/* =============== 画面 ②：学号 + 姓名 =============== */
static void show_id(void)
{
    u8g2_ClearBuffer(&u8g2);
    draw_str_center(&u8g2, 12, "代思雨");
    u8g2_DrawHLine(&u8g2, 16, 32, 96);
    draw_str_center(&u8g2, 40, "632403091001");
    u8g2_SendBuffer(&u8g2);
}

/* =============== 画面 ③：AHT20 温湿度 =============== */
#define AHT_NO_DEV   0      /* 总线上没找到 AHT20 */
#define AHT_OK       1      /* 读到有效数据 */
#define AHT_READ_ERR 2      /* 器件在，但没读到（显示上次的有效值） */

static void show_aht20(uint8_t state, int16_t t_x10, uint16_t h_x10)
{
    char val[8];
    int  x, wl, wv, ws;

    u8g2_ClearBuffer(&u8g2);

    if (state != AHT_OK) {
        /* 字库里只有 25 个汉字，这里用 ASCII，免得出现空白 */
        draw_str_center(&u8g2, 16, "AHT20");
        draw_str_center(&u8g2, 36, state == AHT_NO_DEV ? "NOT FOUND" : "READ FAIL");
        u8g2_SendBuffer(&u8g2);
        return;
    }

    wl = 40;                                /* "温度:" = 2×16 + 8 */
    wv = fmt_x10(val, t_x10) * 8;
    ws = 16;                                /* "℃" */
    x  = (OLED_W - (wl + wv + ws)) / 2;
    draw_str(&u8g2, x, 12, "温度:");
    draw_str(&u8g2, x + wl, 12, val);
    draw_str(&u8g2, x + wl + wv, 12, "℃");

    wl = 40;
    wv = fmt_x10(val, (int16_t)h_x10) * 8;
    ws = 24;                                /* "%RH" */
    x  = (OLED_W - (wl + wv + ws)) / 2;
    draw_str(&u8g2, x, 36, "湿度:");
    draw_str(&u8g2, x + wl, 36, val);
    draw_str(&u8g2, x + wl + wv, 36, "%RH");

    u8g2_SendBuffer(&u8g2);
}

/* =============== 画面 ④：左右滚动 =============== */
static void scroll_text(const char *msg, uint8_t step, uint8_t delay_ms)
{
    const int w = str_width(msg);
    /* 从右侧屏外 (x = w+128) 移到左侧屏外 (x = -w)，每帧左移 step 像素。
     * 帧数要向上取整再多一帧，否则最后会留下几列像素滑不出去。 */
    const int frames = (2 * w + OLED_W + step - 1) / step + 1;

    for (int f = 0; f < frames; f++) {
        u8g2_ClearBuffer(&u8g2);
        draw_str(&u8g2, (w + OLED_W) - f * step, 24, msg);
        u8g2_SendBuffer(&u8g2);
        HAL_Delay(delay_ms);
    }
}

/* =============== 画面 ⑤：上下滚动 ===============
 * 文字从屏幕下方进入，逐行上移到屏幕上方完全消失。
 * 每帧只改 y，配合 draw_bitmap 的负坐标裁剪即可。
 */
static void scroll_text_v(const char *msg, uint8_t step, uint8_t delay_ms)
{
    const int x = (OLED_W - str_width(msg)) / 2;

    for (int y = OLED_H; y >= -16; y -= step) {
        u8g2_ClearBuffer(&u8g2);
        draw_str(&u8g2, x, y, msg);
        u8g2_SendBuffer(&u8g2);
        HAL_Delay(delay_ms);
    }
}

/* =============== 画面 ⑥：头像动画 ===============
 * 64x64 像素头像占左半屏，右半屏竖排姓名。
 * 动画：每 2.4s 眨一次眼（约 220ms），同时整体上浮 1 像素做呼吸感。
 */
static void show_avatar(uint32_t duration_ms)
{
    uint32_t t0 = HAL_GetTick();
    uint32_t t;

    do {
        t = HAL_GetTick() - t0;

        u8g2_ClearBuffer(&u8g2);
        draw_bitmap(&u8g2, 0, ((t / 450u) & 1u) ? -1 : 0, AVATAR_W, AVATAR_H,
                    ((t % 2400u) < 220u) ? avatar_blink : avatar_open);

        draw_str(&u8g2, 84, 4, "代");
        draw_str(&u8g2, 84, 22, "思");
        draw_str(&u8g2, 84, 40, "雨");

        u8g2_SendBuffer(&u8g2);
        HAL_Delay(40);
    } while (t < duration_ms);
}

/* =============== 主函数 =============== */
int main(void)
{
    I2C_HandleTypeDef *aht_bus;
    int16_t  t_x10 = 0;
    uint16_t h_x10 = 0;
    uint8_t  aht_state = AHT_NO_DEV;
    uint8_t  sync[8] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };

    HAL_Init();                         /* SysTick 1ms、NVIC 分组 */
    SystemClock_Config();               /* 72MHz */

    MX_I2C1_Init();                     /* OLED */
    MX_I2C2_Init();                     /* AHT20 */
    MX_USART1_UART_Init();              /* 调试串口 115200 8N1 */

    HAL_Delay(100);                     /* OLED 上电稳定 */

    /* U8g2 初始化：SSD1306 128x64 I2C，全缓冲 f 模式 */
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0,
                                           u8x8_byte_hw_i2c, u8x8_gpio_and_delay);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);

    /* AHT20：先试 PB10/PB11(I2C2)，再试与 OLED 共用的 PB8/PB9(I2C1) */
    aht_bus = AHT20_Begin(&hi2c2, &hi2c1);

    /* 开机先发 8 个 0x55：0x55=01010101，示波器上就是一串等宽方波，
     * 方便测波特率 / 验证串口接线（任务2 用） */
    HAL_UART_Transmit(&huart1, sync, sizeof(sync), 50);
    uart_puts("\r\n[BOOT] OLED + U8g2 demo, UART 115200 8N1\r\n");

    while (1) {
        demo_graphics();                            /* ① U8g2 图形 demo，6s */

        show_id();                                  /* ② 学号姓名 */
        HAL_Delay(3000);

        if (aht_bus != NULL && AHT20_ReadData(aht_bus, &t_x10, &h_x10) == 0) {
            aht_state = AHT_OK;
        } else if (aht_state == AHT_OK) {
            /* 偶发失败沿用上一次的数值，画面不要一闪一闪 */
        } else if (aht_bus != NULL) {
            aht_state = AHT_READ_ERR;
        }
        uart_log_aht((uint8_t)(aht_state == AHT_OK), t_x10, h_x10);
        show_aht20(aht_state, t_x10, h_x10);        /* ③ 温湿度 */
        HAL_Delay(3000);

        scroll_text("Hello，欢迎来到重庆交通大学物联网205实训室！", 2, 8);   /* ④ 左右滚动 */
        scroll_text_v("重庆交通大学", 1, 8);                                 /* ⑤ 上下滚动 */

        show_avatar(6000);                          /* ⑥ 头像动画，6s */
    }
}

/* =============== SysTick（HAL_Delay 时基） =============== */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
