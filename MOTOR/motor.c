#include "motor.h" 
#include "tim.h"

/*
TB6612電機驅動

左電機:
AIN1 -> PB5
AIN2 -> PB6
PWMA -> PA7 TIM3_CH2


右電機:
BIN1 -> PB7
BIN2 -> PB8
PWMB -> PA6 TIM3_CH1

*/

// 電機停止
void Motor_Stop(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8, GPIO_PIN_RESET);

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
}
	
// 初始化PWM
void Motor_Init(void)
{

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

    Motor_Stop();

}

/*
左電機控制

speed:
0~100 正轉
-100~0 反轉
*/
void Left_Motor(int speed)
{
    if(speed > 100)
        speed = 100;

    if(speed < -100)
        speed = -100;

		if(speed >= 0)
    {
        // 正轉
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

    }
    else
    {
        // 反轉
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);

        speed = -speed;

    }
		
 // PWM輸出
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, speed*10);
}

/*
右電機控制

speed:
0~100 正轉
-100~0 反轉
*/
void Right_Motor(int speed)
{
    if(speed > 100)
        speed = 100;

    if(speed < -100)
        speed = -100;

		if(speed >= 0)
    {
        // 正轉
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    }
    else
    {
        // 反轉
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
        speed = -speed;
    }
		
 // PWM輸出
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, speed*10);
}

/*
雙電機速度控制
*/
void Motor_Control(int left_speed,int right_speed)
{
    Left_Motor(left_speed);
    Right_Motor(right_speed);
}

