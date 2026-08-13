#ifndef __TRACK_H
#define __TRACK_H

#include "main.h" 

/*==========================================================
  四路循迹传感器

  从左到右：

  PB0   PB1   PB10   PB11
   L1    L2    R2     R1
==========================================================*/

#define TRACK_L1_PORT    GPIOB
#define TRACK_L1_PIN     GPIO_PIN_0

#define TRACK_L2_PORT    GPIOB
#define TRACK_L2_PIN     GPIO_PIN_1

#define TRACK_R2_PORT    GPIOB
#define TRACK_R2_PIN     GPIO_PIN_10

#define TRACK_R1_PORT    GPIOB
#define TRACK_R1_PIN     GPIO_PIN_11


/* 初始化 */
void TRACK_Init(void);

/* 读取四路传感器 */
void TRACK_Read(uint8_t *L1,
                uint8_t *L2,
                uint8_t *R2,
                uint8_t *R1);

/* 执行一次循迹控制 */
void TRACK_Run(void);

/* 清除循迹历史误差 */
void TRACK_Reset(void);

#endif