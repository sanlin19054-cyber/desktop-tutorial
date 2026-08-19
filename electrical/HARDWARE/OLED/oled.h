#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>
#define OLED_ADDR    (0x3C << 1)

/* SSD1306 I2C 地址 */
#define OLED_ADDR    (0x3C << 1)

/* 初始化 OLED */
void OLED_Init(void);

/* 清屏 */
void OLED_Clear(void);

/* 顯示字符串 */
void OLED_ShowString(uint8_t x, uint8_t y, char *str);

#endif