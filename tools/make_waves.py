# -*- coding: utf-8 -*-
"""Draw protocol timing diagrams (I2C / AHT20 / UART / SPI) as PNG for the lab report."""
from PIL import Image, ImageDraw, ImageFont

OUT = r"D:\Project\STM32Project\stm32-oled-hal\docs"
BG = 255
FG = 0
GREY = 150
LIGHT = 205

F_CJK = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 21)
F_CJK_S = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 17)
F_MONO = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 19)
F_MONO_S = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 16)


def draw_signals(d, slots, x0, slot_w, rows, wire=3):
    """slots: list of per-row levels. rows: list of (y_hi, y_lo)."""
    n = len(slots)
    for r, (y_hi, y_lo) in enumerate(rows):
        prev = slots[0][r]
        y_prev = y_hi if prev else y_lo
        start = 0
        for i in range(1, n):
            cur = slots[i][r]
            if cur != prev:
                x = x0 + i * slot_w
                d.line((x0 + start * slot_w, y_prev, x, y_prev), fill=FG, width=wire)
                y_cur = y_hi if cur else y_lo
                d.line((x, y_prev, x, y_cur), fill=FG, width=wire)
                y_prev, prev, start = y_cur, cur, i
        d.line((x0 + start * slot_w, y_prev, x0 + n * slot_w, y_prev), fill=FG, width=wire)


def label(d, xy, text, font=F_CJK, fill=FG, anchor="la"):
    d.text(xy, text, font=font, fill=fill, anchor=anchor)


def fig_i2c_write():
    slots = [(1, 1)] * 2                      # idle
    slots.append((1, 0))                      # START: SDA falls while SCL high

    def put_byte(b, ack_level):
        for k in range(8):
            bit = (b >> (7 - k)) & 1
            slots.append((0, bit))            # SCL low, SDA set up
            slots.append((1, bit))            # SCL high, data valid
        slots.append((0, 1))                  # SCL low, master releases SDA
        slots.append((1, ack_level))          # SCL high, slave pulls ACK low

    put_byte(0x78, 0)                         # slave address 0x3C<<1 | W
    put_byte(0x00, 0)                         # control byte: following bytes are commands
    put_byte(0xAE, 0)                         # command: display off
    slots.append((0, 0))
    slots.append((1, 0))
    slots.append((1, 1))                      # STOP: SDA rises while SCL high

    slot_w = 13
    left = 92
    scl_hi, scl_lo = 150, 200
    sda_hi, sda_lo = 250, 300
    width = left + len(slots) * slot_w + 30
    img = Image.new("L", (width, 540), BG)
    d = ImageDraw.Draw(img)

    draw_signals(d, slots, left, slot_w, [(scl_hi, scl_lo), (sda_hi, sda_lo)])
    label(d, (20, scl_lo - 18), "SCL", font=F_MONO)
    label(d, (20, sda_lo - 18), "SDA", font=F_MONO)

    bytes_info = [(2, 0x78, "地址 0x78 (W)"),
                  (20, 0x00, "控制字节 0x00"),
                  (38, 0xAE, "命令 0xAE")]
    for bs, val, name in bytes_info:
        for k in range(8):
            x = left + (bs + 2 * k) * slot_w
            d.line((x, scl_hi, x, sda_lo), fill=LIGHT, width=1)
            d.text((x + slot_w, 308), f"b{7 - k}", font=F_MONO_S, fill=GREY, anchor="ma")
        x_ack = left + (bs + 16) * slot_w
        d.line((x_ack, scl_hi, x_ack, sda_lo), fill=LIGHT, width=1)
        d.text((x_ack + slot_w, 332), "ACK", font=F_MONO_S, fill=GREY, anchor="ma")
        d.text((left + (bs + 8) * slot_w, 100), name, font=F_CJK_S, fill=FG, anchor="ma")

    x_start = left + 2 * slot_w
    d.line((x_start, 60, x_start, sda_lo + 6), fill=FG, width=2)
    d.text((x_start + 5, 34), "START", font=F_MONO, fill=FG)
    x_stop = left + (len(slots) - 1) * slot_w
    d.line((x_stop, 60, x_stop, sda_lo + 6), fill=FG, width=2)
    d.text((x_stop - 5, 34), "STOP", font=F_MONO, fill=FG, anchor="ra")

    label(d, (left, 392), "① START：SCL 为高时 SDA 由高变低；② STOP：SCL 为高时 SDA 由低变高。",
          font=F_CJK_S)
    label(d, (left, 422), "③ 第 9 个时钟是应答位 ACK：从机把 SDA 拉低表示“收到”。",
          font=F_CJK_S)
    label(d, (left, 452), "④ SDA 只在 SCL 为低时改变，接收方在 SCL 高电平期间采样。",
          font=F_CJK_S)
    label(d, (left, 492), "400 kHz 快速模式：SCL 周期 2.5 µs，一个字节（含 ACK）约 22.5 µs。",
          font=F_CJK_S, fill=GREY)
    img.save(OUT + r"\fig_i2c_write.png")


def fig_aht():
    img = Image.new("L", (1460, 470), BG)
    d = ImageDraw.Draw(img)
    y = 150
    x0, x1 = 60, 1400

    def block(xa, xb, text, sub):
        d.rectangle((xa, y, xb, y + 80), outline=FG, width=3)
        d.text(((xa + xb) // 2, y + 16), text, font=F_CJK_S, fill=FG, anchor="ma")
        d.text(((xa + xb) // 2, y + 46), sub, font=F_CJK_S, fill=FG, anchor="ma")

    block(x0, 380, "主机写 3 字节", "AC 33 00")
    block(540, 900, "等待测量 80 ms", "HAL_Delay(80)")
    block(1060, x1, "主机读 6 字节", "状态 + 湿度 + 温度")
    for xa, xb in ((384, 536), (904, 1056)):
        d.line((xa, y + 40, xb - 16, y + 40), fill=FG, width=3)
        d.polygon([(xb - 16, y + 31), (xb - 2, y + 40), (xb - 16, y + 49)], fill=FG)
    d.text((x1, 84), "一次完整测量 ≈ 100 ms", font=F_CJK, fill=FG, anchor="ra")
    label(d, (60, y + 120), "0xAC 0x33 0x00 触发一次转换；等 80 ms 后读回 data[0..5]。", font=F_CJK_S)
    label(d, (60, y + 156), "data[0] bit7 = 忙标志：1 表示还没测完，要重读（代码里重试 3 次）。", font=F_CJK_S)
    label(d, (60, y + 192),
          "湿度 = (data[1]<<12 | data[2]<<4 | data[3]>>4) / 2^20 × 100 %", font=F_CJK_S)
    label(d, (60, y + 228),
          "温度 = ((data[3]&0x0F)<<16 | data[4]<<8 | data[5]) / 2^20 × 200 − 50 ℃", font=F_CJK_S)
    img.save(OUT + r"\fig_aht_timing.png")


def fig_uart():
    slots = [(1,)] * 2
    slots.append((0,))                        # start bit
    for k in range(8):                        # LSB first
        slots.append(((0x55 >> k) & 1,))
    slots.append((1,))                        # stop bit
    slots += [(1,)] * 2

    slot_w = 46
    left = 110
    top = 130
    img = Image.new("L", (left + len(slots) * slot_w + 60, 430), BG)
    d = ImageDraw.Draw(img)
    draw_signals(d, slots, left, slot_w, [(top, top + 70)])
    label(d, (22, top + 22), "TX (PA9)", font=F_MONO_S)

    d.text((left + 2 * slot_w + slot_w // 2, 320), "起始位", font=F_CJK_S, fill=FG, anchor="ma")
    d.text((left + 2 * slot_w + slot_w // 2, top - 34), "0", font=F_MONO, fill=FG, anchor="ma")
    for k in range(8):
        cx = left + (3 + k) * slot_w + slot_w // 2
        d.text((cx, 320), f"D{k}", font=F_MONO_S, fill=GREY, anchor="ma")
        d.text((cx, top - 34), str((0x55 >> k) & 1), font=F_MONO, fill=FG, anchor="ma")
        d.line((left + (3 + k) * slot_w, top + 70, left + (3 + k) * slot_w, 312),
               fill=LIGHT, width=1)
    cx = left + 11 * slot_w + slot_w // 2
    d.text((cx, 320), "停止位", font=F_CJK_S, fill=FG, anchor="ma")
    d.text((cx, top - 34), "1", font=F_MONO, fill=FG, anchor="ma")

    d.line((left + 2 * slot_w, top + 100, left + 3 * slot_w, top + 100), fill=FG, width=2)
    for xx in (left + 2 * slot_w, left + 3 * slot_w):
        d.line((xx, top + 93, xx, top + 107), fill=FG, width=2)
    d.text((left + 2 * slot_w + slot_w // 2, top + 114), "1 bit = 8.68 µs @115200",
           font=F_MONO_S, fill=FG, anchor="ma")

    label(d, (left, 360), "0x55 = 0101 0101：数据位上正好是等宽方波，最适合在示波器上核对波特率。",
          font=F_CJK_S)
    label(d, (left, 392), "一帧 10 bit（1 起始 + 8 数据 + 1 停止）= 86.8 µs；空闲时 TX 保持高电平。",
          font=F_CJK_S)
    img.save(OUT + r"\fig_uart_frame.png")


def fig_spi():
    nbits = 8
    slots = []
    for s in range(nbits * 2 + 4):
        cs = 0 if 2 <= s < nbits * 2 + 2 else 1
        sclk = mosi = 0
        if 2 <= s < nbits * 2 + 2:
            k = (s - 2) // 2
            sclk = 1 if (s - 2) % 2 else 0
            mosi = (0x9F >> (7 - k)) & 1
        slots.append((cs, sclk, mosi, 0))

    slot_w = 30
    left = 120
    top = 100
    img = Image.new("L", (left + len(slots) * slot_w + 50, 520), BG)
    d = ImageDraw.Draw(img)
    rows = [(top, top + 40), (top + 90, top + 130), (top + 180, top + 220), (top + 270, top + 310)]
    draw_signals(d, slots, left, slot_w, rows, wire=2)
    for txt, yy in (("CS", top), ("SCLK", top + 90), ("MOSI", top + 180), ("MISO", top + 270)):
        label(d, (24, yy + 6), txt, font=F_MONO_S)
    for k in range(8):
        cx = left + (2 + 2 * k + 1) * slot_w + slot_w // 2
        d.text((cx, top + 320), f"b{7 - k}", font=F_MONO_S, fill=GREY, anchor="ma")
    label(d, (left, 452), "SPI 模式 0：SCLK 上升沿采样、下降沿切换 MOSI，MSB 先出。", font=F_CJK_S)
    label(d, (left, 484), "OLED 用 I2C 只要 2 根线；SPI 更快（10 MHz 以上）但要多占引脚。",
          font=F_CJK_S)
    img.save(OUT + r"\fig_spi_frame.png")


fig_i2c_write()
fig_aht()
fig_uart()
fig_spi()
print("figures written")
