# -*- coding: utf-8 -*-
"""Simulate the SSD1306 128x64 framebuffer with the project's own font tables.

fixed - matches the new main.c (MSB-first bits, per-pixel clipping)
old   - matches the old main.c (u8g2_DrawXBM reads LSB-first, x<0 path read MSB-first)
"""
import re
from PIL import Image, ImageDraw, ImageFont

ROOT = r"D:\Project\STM32Project\stm32-oled-hal"
W, H = 128, 64


def parse_arrays(path):
    txt = open(path, encoding="utf-8").read()
    out = []
    for m in re.finditer(r"\{([^{}]*)\}\s*,?\s*(?:/\*\s*(.*?)\s*\*/)?", txt):
        nums = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1))]
        if len(nums) in (16, 32):
            out.append(nums)
    return out


asc = parse_arrays(ROOT + r"\font_asc8x16.c")
hz_txt = open(ROOT + r"\font_hz16.c", encoding="utf-8").read()
hz = {}
for m in re.finditer(r'\{"(.)",\s*\{([^}]*)\}\}', hz_txt):
    hz[m.group(1)] = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(2))]
assert len(asc) == 95, len(asc)
assert len(hz) == 25, len(hz)


class FB:
    def __init__(self):
        self.px = bytearray(W * H)

    def clear(self):
        self.px = bytearray(W * H)

    def pixel(self, x, y):
        if 0 <= x < W and 0 <= y < H:
            self.px[y * W + x] = 1

    def hline(self, x, y, n):
        for i in range(n):
            self.pixel(x + i, y)

    def glyph(self, x, y, w, data, msb=True):
        if data is None:
            return
        bpr = (w + 7) // 8
        for row in range(16):
            line = data[row * bpr:(row + 1) * bpr]
            for col in range(w):
                bit = (0x80 >> (col & 7)) if msb else (1 << (col & 7))
                if line[col >> 3] & bit:
                    self.pixel(x + col, y + row)


def utf8_chars(s):
    for ch in s:
        yield ch, len(ch.encode("utf-8"))


def str_width(s):
    return sum(8 if n == 1 else 16 for _, n in utf8_chars(s))


def draw_str(fb, x, y, s, msb=True):
    for ch, n in utf8_chars(s):
        if n == 1:
            o = ord(ch)
            fb.glyph(x, y, 8, asc[o - 32] if 32 <= o <= 126 else None, msb)
            x += 8
        else:
            fb.glyph(x, y, 16, hz.get(ch), msb)
            x += 16


def center(fb, y, s, msb=True):
    draw_str(fb, (W - str_width(s)) // 2, y, s, msb)


def fmt_x10(v):
    """Mirror of the C fmt_x10: the integer part must keep every digit."""
    sign = "-" if v < 0 else ""
    v = abs(v)
    return f"{sign}{v // 10}.{v % 10}"


def show_id(fb, msb=True):
    fb.clear()
    center(fb, 12, "代思雨", msb)
    fb.hline(16, 32, 96)
    center(fb, 40, "632403091001", msb)


def show_aht(fb, t, h, msb=True):
    fb.clear()
    for y, label, v, unit, uw in ((12, "温度:", t, "℃", 16), (36, "湿度:", h, "%RH", 24)):
        val = fmt_x10(v)
        wl = 40
        wv = len(val) * 8
        x = (W - (wl + wv + uw)) // 2
        draw_str(fb, x, y, label, msb)
        draw_str(fb, x + wl, y, val, msb)
        draw_str(fb, x + wl + wv, y, unit, msb)


def show_missing(fb, state, msb=True):
    fb.clear()
    center(fb, 16, "AHT20", msb)
    center(fb, 36, "NOT FOUND" if state == 0 else "READ FAIL", msb)


def scroll(fb, msg, x, msb=True):
    fb.clear()
    draw_str(fb, x, 24, msg, msb)


def to_image(fb, scale=3):
    img = Image.new("L", (W, H), 0)
    p = img.load()
    for i, v in enumerate(fb.px):
        if v:
            p[i % W, i // W] = 255
    return img.resize((W * scale, H * scale), Image.NEAREST)


def strip(frames, scale=3, gap=14, titles=None):
    imgs = [to_image(f, scale) for f in frames]
    total_w = sum(i.width for i in imgs) + gap * (len(imgs) - 1)
    extra = 22 if titles else 0
    out = Image.new("L", (total_w, imgs[0].height + extra), 20)
    d = ImageDraw.Draw(out)
    font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 16)
    x = 0
    for i, im in enumerate(imgs):
        out.paste(im, (x, extra))
        if titles:
            d.text((x + 4, 2), titles[i], fill=170, font=font)
        x += im.width + gap
    return out


MSG = "Hello，欢迎来到重庆交通大学物联网205实训室！"
STEP, DELAY = 2, 8
W_MSG = str_width(MSG)
FRAMES = (W_MSG + W + W_MSG) // STEP + 1
print(f"scroll text width = {W_MSG}px, frames = {FRAMES}, "
      f"~{FRAMES * (DELAY + 40) / 1000:.0f}s per pass")

makers = [
    lambda f: show_id(f),
    lambda f: show_aht(f, 253, 625),
    lambda f: show_missing(f, 0),
    lambda f: scroll(f, MSG, 40),
    lambda f: scroll(f, MSG, -6),
]
titles = ["① 学号+姓名", "② 温湿度 25.3/62.5", "② 传感器未接", "③ 滚动中", "③ 左端裁剪"]
frames = []
for mk in makers:
    f = FB()
    mk(f)
    frames.append(f)
strip(frames, 3, 30, titles).save(ROOT + r"\_check\preview_after.png")

# --- before fix: LSB-first lookup, i.e. what u8g2_DrawXBM does --------------
frames = []
for mk in (lambda f: show_id(f, msb=False),
           lambda f: show_aht(f, 253, 625, msb=False),
           lambda f: scroll(f, MSG, 40, msb=False)):
    f = FB()
    mk(f)
    frames.append(f)
strip(frames, 3, 30, ["① 镜像乱码", "② 数字也反了", "③ 滚动一样乱"]).save(
    ROOT + r"\_check\preview_before.png")

raw_t = int((25.0 + 50) / 200 * (1 << 20))
print("raw_t for 25.0C =", raw_t)
print("  new:", ((raw_t * 2000) + 524288) // (1 << 20) - 500, "(x10, correct)")
print("  old:", ((raw_t * 1000) + 524288) // (1 << 20) - 500, "(x10, shows -12.5)")
print("done")
