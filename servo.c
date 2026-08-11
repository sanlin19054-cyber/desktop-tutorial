#include "servo.h"
#include "tim.h" 

// 舵機初始化
void SERVO_Init(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}


// 設定角度
void SERVO_SetAngle(uint8_t angle)
{
    uint16_t pwm;

    // 限制角度
    if(angle > 180)
    {
        angle = 180;
    }

    /*
        0度   500us
        180度 2500us
    */

    pwm = 500 + (angle * 2000 / 180);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pwm);

}