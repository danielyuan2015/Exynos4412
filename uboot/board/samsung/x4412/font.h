#ifndef __FONT_H__
#define __FONT_H__

#include "types.h"

void lcd_textout(u32_t x, u32_t y, u32_t fc, u32_t bc, u8_t * buf, u32_t cnt);
void lcd_print(u32_t x, u32_t y, u32_t fc, u32_t bc, const char * fmt, ...);

#endif /* __FONT_H__ */
