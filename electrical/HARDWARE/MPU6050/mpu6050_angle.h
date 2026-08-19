#ifndef __MPU6050_ANGLE_H
#define __MPU6050_ANGLE_H

void MPU6050_Angle_Init(void);
void MPU6050_Angle_Update(void);

float MPU6050_GetPitch(void);
float MPU6050_GetRoll(void);

#endif