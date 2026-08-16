#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"
void Motor_Stop(void);
void Motor_Init(void);
void Left_Motor(int speed);
void Right_Motor(int speed);
void Motor_Control(int left_speed,int right_speed);

#endif