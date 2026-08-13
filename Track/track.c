#include "track.h" 
#include "motor.h" 

/*==========================================================
  参数设置
==========================================================*/

/*
 * 基础速度
 *
 * 直线时的左右轮基础速度
 */
#define TRACK_BASE_SPEED        40


/*
 * 最大速度
 */
#define TRACK_MAX_SPEED         60


/*
 * 最小速度
 *
 * 不允许正常循迹时突然变成极小值
 */
#define TRACK_MIN_SPEED        -20


/*
 * 比例参数 Kp
 *
 * Kp 越大，修正越激烈
 *
 * Kp 太大，容易左右摆动
 */
#define TRACK_KP                 8

/*
 * 微分参数 Kd
 *
 * Kd 越大：
 *     越能抑制转弯后的过冲
 *
 * Kd 太大：
 *     对传感器噪声比较敏感
 */
#define TRACK_KD                 5

/*
 * 最大修正量
 *
 * 防止某一次错误导致电机突然大幅变化
 */
#define TRACK_MAX_CORRECTION    35

/*
 * 丢线时的速度
 */
#define TRACK_LOST_SPEED        25


/*==========================================================
  上一次误差

  用于 PD 控制
==========================================================*/
static int last_error = 0;


/*==========================================================
  上一次有效误差

  当四个传感器都没有检测到黑线时，
  根据这个值判断应该向哪边寻找线路。
==========================================================*/
static int last_valid_error = 0;


/*==========================================================
  初始化
==========================================================*/
void TRACK_Init(void)
{
    last_error = 0;
    last_valid_error = 0;
}


/*==========================================================
  读取四路传感器
==========================================================*/
void TRACK_Read(uint8_t *L1,
                uint8_t *L2,
                uint8_t *R2,
                uint8_t *R1)
{
    *L1 = HAL_GPIO_ReadPin(TRACK_L1_PORT, TRACK_L1_PIN);
    *L2 = HAL_GPIO_ReadPin(TRACK_L2_PORT, TRACK_L2_PIN);
    *R2 = HAL_GPIO_ReadPin(TRACK_R2_PORT, TRACK_R2_PIN);
    *R1 = HAL_GPIO_ReadPin(TRACK_R1_PORT, TRACK_R1_PIN);
}


/*==========================================================
  限制速度
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
  限制修正量
==========================================================*/
static int TRACK_LimitCorrection(int correction)
{
    if(correction > TRACK_MAX_CORRECTION)
    {
        correction = TRACK_MAX_CORRECTION;
    }

    if(correction < -TRACK_MAX_CORRECTION)
    {
        correction = -TRACK_MAX_CORRECTION;
    }

    return correction;
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
    int correction;

    int left_speed;
    int right_speed;

    int active_count;

    /*------------------------------------------------------
      读取传感器
    ------------------------------------------------------*/
    TRACK_Read(&L1, &L2, &R2, &R1);

    /*------------------------------------------------------
      计算黑线传感器数量
      黑线 = 0
    ------------------------------------------------------*/
    active_count = 0;

    if(L1 == 0)
        active_count++;

    if(L2 == 0)
        active_count++;

    if(R2 == 0)
        active_count++;

    if(R1 == 0)
        active_count++;


    /*======================================================
      情况1：四个都是白色
      → 丢线
    ======================================================*/
    if(active_count == 0)
    {
        /*
         * 不直接停止，也不直接直走。
         *
         * 根据上一次误差决定寻找方向。
         */

        if(last_valid_error < 0)
        {
            /*
             * 上次线路在左边
             * 向左寻找线路
             */
            Motor_Control(
                -TRACK_LOST_SPEED,
                 TRACK_LOST_SPEED
            );
        }
        else if(last_valid_error > 0)
        {
            /*
             * 上次线路在右边
             */
            Motor_Control(
                 TRACK_LOST_SPEED,
                -TRACK_LOST_SPEED
            );
        }
        else
        {
            /*
             * 第一次就丢线
             * 低速直行
             */
            Motor_Control(
                TRACK_LOST_SPEED,
                TRACK_LOST_SPEED
            );
        }

        return;
    }


    /*======================================================
      情况2：四个都检测到黑线
      → 大面积黑线 / 路口

      这里暂时低速直行，避免突然停止造成抖动。
    ======================================================*/
    if(active_count == 4)
    {
        last_error = 0;

        Motor_Control(
            25,
            25
        );

        return;
    }


    /*======================================================
      计算加权误差

      左外   左内   右内   右外
       -3     -1     +1     +3
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


    /*------------------------------------------------------
      保存最后一次有效误差
    ------------------------------------------------------*/
    last_valid_error = error;


    /*======================================================
      PD控制

      correction =
      Kp × error
      +
      Kd × (error - last_error)
    ======================================================*/

    correction =
        TRACK_KP * error
        +
        TRACK_KD * (error - last_error);


    /*------------------------------------------------------
      限制修正量
    ------------------------------------------------------*/
    correction =
        TRACK_LimitCorrection(correction);


    /*------------------------------------------------------
      保存当前误差
    ------------------------------------------------------*/
    last_error = error;


    /*======================================================
      根据误差决定基础速度

      误差越大 → 说明正在转弯
      转弯时主动减速

      这样可以减少冲过黑线后的左右摆动
    ======================================================*/

    int base_speed;


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

      黑线偏左：
          error < 0
          correction < 0
          左轮减速
          右轮加速

      黑线偏右：
          error > 0
          correction > 0
          左轮加速
          右轮减速
    ======================================================*/

    left_speed =
        base_speed + correction;

    right_speed =
        base_speed - correction;


    /*------------------------------------------------------
      限制最终速度
    ------------------------------------------------------*/
    left_speed =
        TRACK_LimitSpeed(left_speed);

    right_speed =
        TRACK_LimitSpeed(right_speed);


    /*------------------------------------------------------
      输出到电机
    ------------------------------------------------------*/
    Motor_Control(
        left_speed,
        right_speed
    );
}


/*==========================================================
  清除历史误差
==========================================================*/
void TRACK_Reset(void)
{
    last_error = 0;
    last_valid_error = 0;

    Motor_Stop();
}