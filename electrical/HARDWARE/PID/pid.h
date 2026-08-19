#ifndef __PID_H
#define __PID_H

#include "main.h"

typedef struct
{
    float Kp;
    float Ki;
    float Kd;

    float error;
    float last_error;

    float integral;

    float output;

    float output_max;
    float output_min;

} PID_TypeDef;

/* PID 初始化 */
void PID_Init(PID_TypeDef *pid,
              float kp,
              float ki,
              float kd,
              float output_min,
              float output_max);

/* PID 計算 */
float PID_Calculate(PID_TypeDef *pid, float error);

/* 清除 PID 狀態 */
void PID_Reset(PID_TypeDef *pid);

#endif