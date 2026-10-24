#ifndef FONT_HZ16_H
#define FONT_HZ16_H
#include <stdint.h>
typedef struct { const char name[4]; const uint8_t data[32]; } Font16_t;
extern const Font16_t font16_tab[];
extern const uint8_t font16_count;
/* 按汉字查找 16x16 点阵, 找不到返回 NULL */
const uint8_t *font16_find(const char *hz);
#endif
