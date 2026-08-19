#ifndef __MPU6050_H
#define __MPU6050_H
#define M_PI 3.14

#include "main.h" 
#include "i2c.h"

#define MPU6050_ADDR 0x68<<1

void MPU6050_Write(uint8_t reg,uint8_t data);
void MPU6050_Read(uint8_t reg, uint8_t *data, uint8_t len);
void MPU6050_Init(void);
void MPU6050_ReadAccel(int16_t *Ax, int16_t *Ay, int16_t *Az);
void MPU6050_ReadGyro(int16_t *Gx, int16_t *Gy, int16_t *Gz);

#endif