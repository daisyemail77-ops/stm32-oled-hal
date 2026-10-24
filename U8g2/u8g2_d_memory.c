/* 由 prune_u8g2.py 自动裁剪 - 仅 SSD1306 I2C 128x64 noname */
#include "u8g2.h"

uint8_t *u8g2_m_16_8_f(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 8;
  return 0;
  #else
  static uint8_t buf[1024];
  *page_cnt = 8;
  return buf;
  #endif
}

