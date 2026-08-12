#include "mpu6050.h" 
#include "mpu6050.h" 
#include <math.h> 

float Pitch = 0;
float Roll = 0;

void MPU6050_Angle_Update(void)
{
    int16_t Ax,Ay,Az;
    int16_t Gx,Gy,Gz;

    float AccPitch;
    float AccRoll;
    float dt = 0.01f;

    //讀取數據
    MPU6050_ReadAccel(&Ax, &Ay, &Az);
    MPU6050_ReadGyro(&Gx, &Gy, &Gz);

    /*
       加速度計算角度
    */

    AccPitch =
    atan2(Ax, sqrt(Ay*Ay+Az*Az))
    *180/M_PI;

    AccRoll =
    atan2(Ay, sqrt(Ax*Ax+Az*Az))
    *180/M_PI;

    /*
       陀螺儀積分
    */
    Pitch += (Gx/16.4f)*dt;
    Roll += (Gy/16.4f)*dt;

    /*
       互補濾波
    */
    Pitch = Pitch*0.98f + AccPitch*0.02f;
    Roll = Roll*0.98f + AccRoll*0.02f;
}

float MPU6050_GetPitch(void)
{
    return Pitch;
}

float MPU6050_GetRoll(void)
{
    return Roll;
}