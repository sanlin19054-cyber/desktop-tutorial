#ifndef __AVOIDANCE_H
#define __AVOIDANCE_H

#include "main.h"

/*
 * 避障模块初始化
 */
void Avoidance_Init(void);

/*
 * 执行一次避障判断
 *
 * head  : 前方距离(cm)
 * left  : 左侧距离(cm)
 * right : 右侧距离(cm)
 */
void Avoidance_Run(uint32_t head,
                   uint32_t left,
                   uint32_t right);

#endif