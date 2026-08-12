#include "mpu6050.h" 

extern I2C_HandleTypeDef hi2c1;

// 寄存器
void MPU6050_Write(uint8_t reg, uint8_t data)
{
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, reg, 1, &data, 1, 100);
}

void MPU6050_Read(uint8_t reg, uint8_t *data, uint8_t len)
{
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, reg, 1, data, len, 100);
}

// 初始化
void MPU6050_Init(void)
{
    //解除睡眠模式
    MPU6050_Write(0x6B, 0x00);
	
	 //陀螺儀 ±2000dps
    MPU6050_Write(0x1B, 0x18);

    //加速度 ±2g
    MPU6050_Write(0x1C, 0x00);
}
	
//讀加速度
void MPU6050_ReadAccel(int16_t *Ax, int16_t *Ay, int16_t *Az)
{
    uint8_t data[6];

    MPU6050_Read(0x3B, data, 6);

    *Ax =
    (data[0]<<8)|data[1];

    *Ay =
    (data[2]<<8)|data[3];

    *Az =
    (data[4]<<8)|data[5];
}



//讀陀螺儀
void MPU6050_ReadGyro(int16_t *Gx, int16_t *Gy, int16_t *Gz)
{
    uint8_t data[6];

    MPU6050_Read(0x43, data, 6);

    *Gx =
    (data[0]<<8)|data[1];

    *Gy =
    (data[2]<<8)|data[3];

    *Gz =
    (data[4]<<8)|data[5];
}