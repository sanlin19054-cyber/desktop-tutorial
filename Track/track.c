#include "track.h"
#include "motor.h"
#include "pid.h"


/*==========================================================
  参数设置
==========================================================*/

/*
 * 直线基础速度
 */
#define TRACK_BASE_SPEED        40

/*
 * 最大电机速度
 */
#define TRACK_MAX_SPEED         60

/*
 * 最小电机速度
 */
#define TRACK_MIN_SPEED        -20

/*
 * PID 最大修正量
 */
#define TRACK_MAX_CORRECTION    35

/*
 * 丢线时的速度
 */
#define TRACK_LOST_SPEED        25


/*==========================================================
  PID 控制器
==========================================================*/

static PID_TypeDef track_pid;


/*==========================================================
  上一次有效误差
 *
 * 用于丢线后判断向左还是向右寻找黑线
==========================================================*/

static int last_valid_error = 0;


/*==========================================================
  TRACK 初始化
==========================================================*/

void TRACK_Init(void)
{
    last_valid_error = 0;

    /*
     * PID 参数
     *
     * Kp = 8
     * Ki = 0
     * Kd = 5
     *
     * 先使用 PD 控制，
     * Ki 暂时关闭。
     */

    PID_Init(&track_pid, 8.0f, 0.0f, 5.0f, -35.0f, 35.0f);
}


/*==========================================================
  读取四路传感器
==========================================================*/

void TRACK_Read(uint8_t *L1, uint8_t *L2, uint8_t *R2, uint8_t *R1)
{
    *L1 = HAL_GPIO_ReadPin(TRACK_L1_PORT, TRACK_L1_PIN);

    *L2 = HAL_GPIO_ReadPin(TRACK_L2_PORT, TRACK_L2_PIN);

    *R2 = HAL_GPIO_ReadPin(TRACK_R2_PORT, TRACK_R2_PIN);

    *R1 = HAL_GPIO_ReadPin(TRACK_R1_PORT, TRACK_R1_PIN);
}


/*==========================================================
  限制电机速度
==========================================================*/

static int TRACK_LimitSpeed(int speed)
{
    if(speed > TRACK_MAX_SPEED)
    {
        speed = TRACK_MAX_SPEED;
    }

    if(speed < TRACK_MIN_SPEED)
    {
        speed = TRACK_MIN_SPEED;
    }

    return speed;
}


/*==========================================================
  执行一次循迹
==========================================================*/

void TRACK_Run(void)
{
    uint8_t L1;
    uint8_t L2;
    uint8_t R2;
    uint8_t R1;

    int error;
    int left_speed;
    int right_speed;
    int active_count;
    int base_speed;

    float correction;


    /*======================================================
      读取传感器
    ======================================================*/

    TRACK_Read(&L1, &L2, &R2, &R1);


    /*======================================================
      统计检测到黑线的传感器数量

      黑线 = 0
    ======================================================*/

    active_count = 0;

    if(L1 == 0)
    {
        active_count++;
    }

    if(L2 == 0)
    {
        active_count++;
    }

    if(R2 == 0)
    {
        active_count++;
    }

    if(R1 == 0)
    {
        active_count++;
    }


    /*======================================================
      情况1：
      四个传感器都是白色

      → 丢线
    ======================================================*/

    if(active_count == 0)
    {
        /*
         * 根据上一次有效误差
         * 判断向哪边寻找黑线
         */

        if(last_valid_error < 0)
        {
            /*
             * 黑线之前在左边
             *
             * 左转寻找
             */

            Motor_Control(-TRACK_LOST_SPEED, TRACK_LOST_SPEED);
        }
        else if(last_valid_error > 0)
        {
            /*
             * 黑线之前在右边
             *
             * 右转寻找
             */

            Motor_Control(TRACK_LOST_SPEED, -TRACK_LOST_SPEED);
        }
        else
        {
            /*
             * 第一次就丢线
             *
             * 低速直行
             */

            Motor_Control(TRACK_LOST_SPEED, TRACK_LOST_SPEED);
        }
        return;
    }


    /*======================================================
      情况2：
      四个传感器全部检测到黑线

      → 大面积黑线 / 路口
    ======================================================*/

    if(active_count == 4)
    {
        PID_Reset(&track_pid);
        last_valid_error = 0;

        /*
         * 低速通过
         */

        Motor_Control(25, 25);
        return;
    }


    /*======================================================
      计算循迹误差

      从左到右：

      L1    L2    R2    R1
      -3    -1    +1    +3
    ======================================================*/

    error = 0;

    if(L1 == 0)
    {
        error -= 3;
    }

    if(L2 == 0)
    {
        error -= 1;
    }

    if(R2 == 0)
    {
        error += 1;
    }

    if(R1 == 0)
    {
        error += 3;
    }


    /*======================================================
      保存最后一次有效误差
    ======================================================*/

    last_valid_error = error;


    /*======================================================
      PID 计算

      correction：
      正数 → 向右修正
      负数 → 向左修正
    ======================================================*/

    correction =
        PID_Calculate(&track_pid, (float)error);


    /*======================================================
      根据误差调整基础速度

      大弯 → 减速
      小弯 → 中速
      直线 → 正常速度

      这样可以减少转弯后的左右抖动
    ======================================================*/

    if(error <= -2 || error >= 2)
    {
        /*
         * 大弯
         */
        base_speed = 30;
    }
    else if(error != 0)
    {
        /*
         * 小弯
         */
        base_speed = 35;
    }
    else
    {
        /*
         * 直线
         */
        base_speed = TRACK_BASE_SPEED;
    }


    /*======================================================
      差速控制

      左轮 = 基础速度 + PID修正
      右轮 = 基础速度 - PID修正
    ======================================================*/

    left_speed =
        base_speed + (int)correction;

    right_speed =
        base_speed - (int)correction;


    /*======================================================
      限制最终速度
    ======================================================*/

    left_speed =
        TRACK_LimitSpeed(left_speed);

    right_speed =
        TRACK_LimitSpeed(right_speed);


    /*======================================================
      控制电机
    ======================================================*/

    Motor_Control(left_speed, right_speed);
}


/*==========================================================
  清除循迹状态
==========================================================*/

void TRACK_Reset(void)
{
    last_valid_error = 0;
    PID_Reset(&track_pid);
    Motor_Stop();
}