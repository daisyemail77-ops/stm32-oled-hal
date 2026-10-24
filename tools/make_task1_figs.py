# -*- coding: utf-8 -*-
"""Figures for 任务1: SSD1306 memory layout + 16x16 glyph -> byte mapping."""
import re
from PIL import Image, ImageDraw, ImageFont

ROOT = r"D:\Project\STM32Project\stm32-oled-hal"
BG, FG, GREY, LIGHT = 255, 0, 130, 205

F_CN = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 20)
F_CN_S = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 16)
F_MONO = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 17)
F_MONO_S = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 14)

hz_txt = open(ROOT + r"\font_hz16.c", encoding="utf-8").read()
glyphs = {}
for m in re.finditer(r'\{"(.)",\s*\{([^}]*)\}\}', hz_txt):
    glyphs[m.group(1)] = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(2))]


def fig_glyph_mapping(ch="代", cell=26):
    data = glyphs[ch]
    grid = 16 * cell
    left, top = 90, 78
    text_x = left + grid + 60
    width = text_x + 620
    height = top + grid + 110
    img = Image.new("L", (width, height), BG)
    d = ImageDraw.Draw(img)

    # grid
    for i in range(17):
        d.line((left + i * cell, top, left + i * cell, top + grid), fill=LIGHT, width=1)
        d.line((left, top + i * cell, left + grid, top + i * cell), fill=LIGHT, width=1)

    # pixels
    for row in range(16):
        hi, lo = data[row * 2], data[row * 2 + 1]
        for col in range(16):
            bit = (hi >> (7 - col)) if col < 8 else (lo >> (15 - col))
            if bit & 1:
                x0 = left + col * cell
                y0 = top + row * cell
                d.rectangle((x0 + 1, y0 + 1, x0 + cell - 2, y0 + cell - 2), fill=FG)

    # labels
    for c in range(16):
        d.text((left + c * cell + cell // 2, top - 24), str(c), font=F_MONO_S,
               fill=GREY, anchor="ma")
    for r in range(16):
        d.text((left - 12, top + r * cell + cell // 2), str(r), font=F_MONO_S,
               fill=GREY, anchor="rm")

    d.text((left + grid // 2, 22), "16×16 点阵（每个格子 = 1 个像素，列号 0~15）",
           font=F_CN, fill=FG, anchor="ma")

    # byte table on the right
    d.text((text_x, top - 46), "每行 2 字节，MSB(bit7) 是最左边的像素", font=F_CN_S, fill=FG)
    for row in range(16):
        y = top + row * cell + 4
        hi, lo = data[row * 2], data[row * 2 + 1]
        bits = f"{hi:08b} {lo:08b}"
        d.text((text_x, y), f"行{row:>2}", font=F_MONO_S, fill=GREY)
        d.text((text_x + 44, y - 2), f"0x{hi:02X} 0x{lo:02X}", font=F_MONO, fill=FG)
        d.text((text_x + 172, y - 2), bits, font=F_MONO_S, fill=GREY)

    # highlight row 0 -> its two bytes
    d.rectangle((left, top, left + grid, top + cell), outline=FG, width=2)
    d.line((left + grid + 4, top + cell // 2, text_x - 6, top + cell // 2), fill=FG, width=2)

    d.text((text_x, top + grid + 26),
           "整字 = 16 行 × 2 字节 = 32 字节。第 0 行 = 0x08,0x00 → 0000 1000 0000 0000，",
           font=F_CN_S, fill=FG)
    d.text((text_x, top + grid + 54),
           "只有第 4 列亮，因为 bit7 对应第 0 列、bit3 对应第 4 列。",
           font=F_CN_S, fill=FG)
    img.save(ROOT + r"\docs\fig_hz16_mapping.png")
    print("fig_hz16_mapping.png")


def fig_oled_mem():
    # 128x64 organised as 8 pages of 8 rows; draw 128 columns x 8 pages
    cw, ph = 5, 34            # column width px, page height px
    left, top = 90, 96
    cols, pages = 128, 8
    width = left + cols * cw + 540
    height = top + pages * ph + 120
    img = Image.new("L", (width, height), BG)
    d = ImageDraw.Draw(img)

    for p in range(pages):
        y = top + p * ph
        d.rectangle((left, y, left + cols * cw, y + ph), outline=LIGHT)
        d.text((left - 14, y + ph // 2), f"PAGE{p}", font=F_MONO_S, fill=GREY, anchor="rm")

    # highlight page 0 and column 32
    d.rectangle((left, top, left + cols * cw, top + ph), outline=FG, width=2)
    col_x = left + 32 * cw
    d.rectangle((col_x, top, col_x + cw, top + pages * ph), fill=LIGHT)
    d.rectangle((col_x, top, col_x + cw, top + pages * ph), outline=FG, width=2)

    # column labels
    for c in (0, 32, 63, 95, 127):
        d.text((left + c * cw + cw // 2, top - 14), str(c), font=F_MONO_S, fill=GREY, anchor="ma")
    d.text((left + cols * cw // 2, 26), "SSD1306 显存 128 列 × 8 页（每页 8 行）",
           font=F_CN, fill=FG, anchor="ma")
    d.text((left + cols * cw // 2, 54), "GDRAM = 128 × 64 bit = 1024 字节", font=F_CN_S,
           fill=GREY, anchor="ma")

    # zoom: one column = 8 pixels, bit0 top
    zx, zy, zs = left + cols * cw + 90, top, 30
    d.text((zx - 30, zy - 44), "一列 = 1 字节 = 8 个纵向像素", font=F_CN_S, fill=FG)
    for b in range(8):
        y = zy + b * zs
        d.rectangle((zx, y, zx + zs, y + zs), outline=FG)
        d.text((zx + zs // 2, y + zs // 2), f"b{b}", font=F_MONO_S, fill=GREY, anchor="mm")
    d.text((zx + zs + 10, zy + zs // 2), "← 该页最上面", font=F_CN_S, fill=GREY, anchor="lm")
    d.text((zx + zs + 10, zy + 7 * zs + zs // 2), "← 该页最下面", font=F_CN_S, fill=GREY,
           anchor="lm")
    d.text((zx - 30, zy + 8 * zs + 16), "bit0 在该页最上面、bit7 在最下面", font=F_CN_S,
           fill=GREY)
    d.text((zx - 30, zy + 8 * zs + 44), "（注意：显存是“纵向位序”，和字模的“MSB 在左”方向不同）",
           font=F_CN_S, fill=GREY)

    d.text((left, height - 74),
           "写显存前先发命令 0x21(列地址 0~127) 和 0x22(页地址 0~7)，再发 0x40 + 数据，",
           font=F_CN_S, fill=FG)
    d.text((left, height - 46),
           "地址会自动递增，所以 1024 字节可以一次连续写完 —— 这就是“硬件刷屏”的基础。",
           font=F_CN_S, fill=FG)
    img.save(ROOT + r"\docs\fig_ssd1306_mem.png")
    print("fig_ssd1306_mem.png")


fig_oled_mem()
fig_glyph_mapping("代")
