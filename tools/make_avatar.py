# -*- coding: utf-8 -*-
"""Draw a 64x64 pixel-art avatar (two frames: eyes open / blinking) and export C arrays.

Output:
  avatar.h  - two static const uint8_t[512] arrays, MSB-first, 8 bytes per row
  avatar_preview.png - 8x zoom, both frames side by side
"""
from PIL import Image, ImageDraw

S = 64          # canvas
SCALE = 8


def draw_face(eyes_open: bool) -> Image.Image:
    img = Image.new("1", (S, S), 0)
    d = ImageDraw.Draw(img)

    # ---- hair: filled dome, then carve the face out of it ----
    d.pieslice((11, -12, 53, 30), 180, 360, fill=1)
    d.ellipse((17, 11, 47, 45), fill=0)                 # face interior
    d.ellipse((17, 11, 47, 45), outline=1, width=2)     # face outline

    # ---- hair strands: black strokes over the white hair, so they show up ----
    d.line((21, 8, 29, 14), fill=0, width=2)
    d.line((43, 8, 35, 14), fill=0, width=2)
    d.line((32, 5, 32, 9), fill=0, width=1)

    # ---- ears ----
    d.arc((12, 21, 20, 37), 90, 270, fill=1, width=2)
    d.arc((44, 21, 52, 37), 270, 90, fill=1, width=2)

    # ---- eyebrows ----
    d.line((21, 18, 28, 17), fill=1, width=2)
    d.line((36, 17, 43, 18), fill=1, width=2)

    # ---- eyes ----
    if eyes_open:
        for cx in (24, 40):
            d.ellipse((cx - 4, 22, cx + 4, 29), fill=1)     # sclera
            d.rectangle((cx - 1, 23, cx + 1, 27), fill=0)   # pupil
    else:
        for cx in (24, 40):
            d.line((cx - 4, 26, cx + 4, 26), fill=1, width=2)
            d.line((cx - 3, 28, cx + 3, 28), fill=1, width=1)

    # ---- nose ----
    d.line((32, 28, 32, 33), fill=1, width=1)
    d.line((30, 34, 34, 34), fill=1, width=1)

    # ---- mouth: hollow crescent (a smile with thickness) ----
    d.pieslice((26, 31, 38, 41), 0, 180, fill=1)
    d.pieslice((27, 30, 37, 38), 0, 180, fill=0)

    # ---- neck + shoulders ----
    d.line((28, 45, 28, 50), fill=1, width=2)
    d.line((36, 45, 36, 50), fill=1, width=2)
    d.arc((4, 46, 60, 88), 180, 360, fill=1, width=3)
    d.line((5, 63, 59, 63), fill=1, width=2)

    # ---- collar V ----
    d.line((27, 51, 32, 57), fill=1, width=2)
    d.line((37, 51, 32, 57), fill=1, width=2)
    return img


def to_c_rows(img: Image.Image):
    px = img.load()
    rows = []
    for y in range(S):
        for bx in range(S // 8):
            b = 0
            for bit in range(8):
                if px[bx * 8 + bit, y]:
                    b |= 0x80 >> bit
            rows.append(b)
    return rows


def fmt_array(name: str, data) -> str:
    out = [f"static const uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), 16):
        out.append("    " + ",".join(f"0x{v:02X}" for v in data[i:i + 16]) + ",")
    out.append("};")
    return "\n".join(out)


open_img = draw_face(True)
blink_img = draw_face(False)

# preview
prev = Image.new("1", (S * 2 + 8, S), 0)
prev.paste(open_img, (0, 0))
prev.paste(blink_img, (S + 8, 0))
prev.resize(((S * 2 + 8) * SCALE, S * SCALE), Image.NEAREST).save(
    r"D:\Project\STM32Project\stm32-oled-hal\_check\avatar_preview.png")

header = f"""/* avatar.h - 64x64 像素头像点阵（由 _check/make_avatar.py 生成，勿手改）
 *
 * 格式：逐行存放，每行 8 字节，最高位(0x80)是左边的像素，共 64 行 = 512 字节。
 * 与工程里 16x16 汉字字模的位序一致（MSB 在左），用 draw_bitmap() 绘制。
 * avatar_open  ：睁眼
 * avatar_blink ：闭眼（眨眼动画用）
 */
#ifndef AVATAR_H_
#define AVATAR_H_

#include <stdint.h>

#define AVATAR_W 64
#define AVATAR_H 64

{fmt_array("avatar_open", to_c_rows(open_img))}

{fmt_array("avatar_blink", to_c_rows(blink_img))}

#endif /* AVATAR_H_ */
"""
with open(r"D:\Project\STM32Project\stm32-oled-hal\avatar.h", "w", encoding="utf-8") as f:
    f.write(header)
print("avatar.h written:", 2 * 512, "bytes of bitmap")
