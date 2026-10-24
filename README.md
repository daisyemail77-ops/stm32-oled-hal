# stm32-oled-hal

STM32F103C8T6 + 0.96 寸 SSD1306 OLED（I2C）+ AHT20 的 HAL 工程，U8g2 图形库裁剪移植，
包含 U8g2 图形 demo、学号姓名、温湿度、上下/左右滚动、64×64 头像动画，以及配套的实验报告。

![六个画面](docs/fig_screens.png)

## 功能

| # | 画面 | 说明 |
|---|---|---|
| ① | U8g2 demo | 边框、标题、弹跳小球、进度条（DrawFrame / DrawLine / DrawDisc / DrawBox） |
| ② | 学号姓名 | 代思雨 / 632403091001，居中 + 分隔线 |
| ③ | 温湿度 | AHT20：温度 xx.x℃、湿度 xx.x%RH；未接时显示 `AHT20 / NOT FOUND` |
| ④ | 左右滚动 | “Hello，欢迎来到重庆交通大学物联网205实训室！”逐帧左移 |
| ⑤ | 上下滚动 | “重庆交通大学”从下往上滑过屏幕 |
| ⑥ | 头像动画 | 64×64 像素头像，眨眼 + 轻微浮动，右侧竖排姓名 |

## 硬件连接

| 器件 | 信号 | 引脚 | 说明 |
|---|---|---|---|
| OLED (SSD1306, 0x3C) | SCL / SDA | PB8 / PB9 | I2C1 重映射（需先开 AFIO 时钟） |
| AHT20 (0x38) | SCL / SDA | PB10 / PB11 | I2C2；也可与 OLED 共用 PB8/PB9，程序自动探测 |
| 调试串口 | TX / RX | PA9 / PA10 | USART1 115200 8N1，可接逻辑分析仪看波形 |

两个模块都自带 4.7 kΩ 上拉，接 3.3 V 即可，**不要接 5 V**。

## 目录结构

```
main.c                主程序：6 个画面 + UART 日志 + 时钟/I2C/UART 初始化
aht20.c/.h            AHT20 驱动（双总线探测、初始化与读取重试）
font_hz16.c/.h        16×16 汉字点阵（25 个汉字）
font_asc8x16.c/.h     8×16 ASCII 点阵（95 个字符）
avatar.h              64×64 头像点阵（睁眼 / 闭眼两帧）
U8g2/                 裁剪后的 u8g2 源码（无内置字库）
HAL/ Device/ RTE/     STM32F1 HAL、启动文件与 CMSIS
docs/                 报告用图（协议波形、屏幕截图）
tools/                生成脚本（字模/波形/预览，Python + Pillow）
main.uvprojx          Keil MDK 工程（AC6）
实验报告.md / .pdf     实验报告（含任务 1~3）
博客.md               博客稿
修改说明.md           任务 1 的 bug 修复记录
```

## 编译与下载

1. Keil MDK 打开 `main.uvprojx`，F7 编译（AC6，`-O1`，已加 `--split_sections`），F8 下载。
2. 目标：STM32F103C8Tx，HSE 8 MHz × PLL9 = 72 MHz。没有外部晶振时程序会自动退回内部 HSI 64 MHz。
3. 资源占用：RO ≈ 24.5 KB，RW ≈ 3.06 KB。

## 显示轮播

`main()` 的 `while (1)` 里依次执行 6 个画面：

```
demo_graphics()  6 s  →  show_id()  3 s  →  show_aht20()  3 s
→  scroll_text()（左右，约 20 s）→  scroll_text_v()（上下，约 4 s）
→  show_avatar()  6 s  →  循环
```

想改滚动速度，调 `scroll_text(msg, step, delay_ms)` 的 `step`（每帧像素）和 `delay_ms`。

## 换成自己的头像

```bash
python tools/make_avatar.py     # 修改其中的绘图代码即可，会重新生成 avatar.h
```

`avatar.h` 的格式：64 行 × 8 字节，每字节最高位是最左边的像素，与汉字字模位序一致。

## 生成报告 PDF

```bash
python tools/md2html.py 实验报告.md _report.html "实验6 实验报告"
# 用浏览器打开 _report.html，Ctrl+P → 另存为 PDF（A4）
```

## 常见问题

| 现象 | 原因 |
|---|---|
| 屏幕全黑 | 检查 OLED 的 SCL/SDA 是否接在 PB8/PB9，以及是否使能了 I2C1 重映射 |
| 花屏 / 乱码 | I2C 事务被拆成单字节发送（控制字节和数据的 START/STOP 被分开） |
| 字是镜像的 | 字模位序与 `u8g2_DrawXBM` 的 XBM 格式（LSB 在左）不一致 |
| 温度恒为负 | 温度换算少了 ×2（`raw*2000/2^20-500`） |
| 温湿度显示 NOT FOUND | AHT20 没接好，或上拉电阻缺失（可先看串口有没有 `[AHT20]` 输出） |

## License

示例代码，可自由用于课程作业与学习。
