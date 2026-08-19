#include "pid.h" 


/*==========================================================
  PID 初始化
==========================================================*/

void PID_Init(PID_TypeDef *pid, float kp, float ki, float kd, float output_min, float output_max)
{
    if(pid == NULL)
    {
        return;
    }

    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;

    pid->error = 0.0f;
    pid->last_error = 0.0f;

    pid->integral = 0.0f;

    pid->output = 0.0f;

    pid->output_min = output_min;
    pid->output_max = output_max;
}


/*==========================================================
  PID 計算
==========================================================*/

float PID_Calculate(PID_TypeDef *pid, float error)
{
    float derivative;

    if(pid == NULL)
    {
        return 0.0f;
    }

    /*------------------------------------------------------
      保存目前誤差
    ------------------------------------------------------*/

    pid->error = error;

    /*------------------------------------------------------
      P：比例
    ------------------------------------------------------*/

    float P = pid->Kp * pid->error;

    /*------------------------------------------------------
      I：積分

      累積之前的誤差
    ------------------------------------------------------*/

    pid->integral += pid->error;

    /*------------------------------------------------------
      防止積分過大
      避免 Integral Windup
    ------------------------------------------------------*/

    if(pid->integral > 100.0f)
    {
        pid->integral = 100.0f;
    }

    if(pid->integral < -100.0f)
    {
        pid->integral = -100.0f;
    }

    float I = pid->Ki * pid->integral;

    /*------------------------------------------------------
      D：微分

      判斷誤差變化速度
    ------------------------------------------------------*/

    derivative =
        pid->error - pid->last_error;

    float D =
        pid->Kd * derivative;

    /*------------------------------------------------------
      PID 輸出
    ------------------------------------------------------*/

    pid->output =
        P + I + D;

    /*------------------------------------------------------
      輸出限幅
    ------------------------------------------------------*/

    if(pid->output > pid->output_max)
    {
        pid->output = pid->output_max;
    }

    if(pid->output < pid->output_min)
    {
        pid->output = pid->output_min;
    }

    /*------------------------------------------------------
      保存本次誤差
    ------------------------------------------------------*/

    pid->last_error =
        pid->error;

    return pid->output;
}


/*==========================================================
  PID 重置
==========================================================*/

void PID_Reset(PID_TypeDef *pid)
{
    if(pid == NULL)
    {
        return;
    }

    pid->error = 0.0f;
    pid->last_error = 0.0f;

    pid->integral = 0.0f;

    pid->output = 0.0f;
}