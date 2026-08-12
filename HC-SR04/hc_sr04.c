#include "hc_sr04.h" 
#include "tim.h"

/*==========================================================
  DWT 微秒计时初始化
  STM32F103C8T6 主频 72MHz
  DWT->CYCCNT 每个 CPU cycle 计数一次
==========================================================*/

static void DWT_Delay_Init(void)
{
    /* 开启 DWT */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 开启 CYCCNT */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 清零计数器 */
    DWT->CYCCNT = 0;
}

/*==========================================================
  DWT 微秒延时
==========================================================*/
static void DWT_Delay_us(uint32_t us)
{
    uint32_t start;
    uint32_t cycles;

    start = DWT->CYCCNT;

    /*
        计算需要等待多少个 CPU cycle

        72MHz:
        1us = 72个cycle
    */
    cycles = us * (HAL_RCC_GetHCLKFreq() / 1000000U);

    while ((DWT->CYCCNT - start) < cycles)
    {
        /* 等待 */
			__NOP();
    }
}

/*==========================================================
  HC-SR04 初始化
==========================================================*/
void HC_SR04_Init(void)
{
    /* 初始化 DWT */
    DWT_Delay_Init();

    /* TRIG 初始为低电平 */
    HAL_GPIO_WritePin(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN, GPIO_PIN_RESET);
}



/*==========================================================
  HC-SR04 测距

  返回：
      距离，单位 cm

  返回 0：
      表示测量失败或超时
==========================================================*/
uint32_t HC_SR04_GetDistance(void)
{
    uint32_t start;
    uint32_t cycles;
    uint32_t time_us;
    uint32_t timeout;

    /*======================================================
      1. 发送 TRIG 触发脉冲

      HC-SR04 需要至少约 10us 高电平
    ======================================================*/

    HAL_GPIO_WritePin(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN, GPIO_PIN_SET);
    DWT_Delay_us(10);
    HAL_GPIO_WritePin(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN, GPIO_PIN_RESET);

    /*======================================================
      2. 等待 ECHO 变高
    ======================================================*/

    timeout = HAL_GetTick();

    while (
        HAL_GPIO_ReadPin(HC_SR04_ECHO_PORT, HC_SR04_ECHO_PIN) == GPIO_PIN_RESET
					)
    {
        /*
            防止一直等下去

            超过50ms认为测量失败
        */
        if ((HAL_GetTick() - timeout) > 50U)
        {
            return 0;
        }
    }

    /*======================================================
      3. ECHO变高，开始计时
    ======================================================*/
    start = DWT->CYCCNT;

    /*======================================================
      4. 等待 ECHO 变低
    ======================================================*/
    while (
        HAL_GPIO_ReadPin(HC_SR04_ECHO_PORT, HC_SR04_ECHO_PIN) == GPIO_PIN_SET
					)
    {
        /*
            防止异常情况下无限等待
			
            这里约限制到30ms
        */
        if (
            (DWT->CYCCNT - start) >
            (HAL_RCC_GetHCLKFreq() / 1000U * 30U)
        )
        {
            return 0;
        }
    }


    /*======================================================
      5. 计算 ECHO 高电平持续时间
    ======================================================*/

    cycles = DWT->CYCCNT - start;

    /*
        CPU cycle → 微秒

        72MHz:
        72 cycle = 1us
    */
    time_us =
        cycles /
        (HAL_RCC_GetHCLKFreq() / 1000000U);


    /*======================================================
      6. 计算距离

      HC-SR04：

      距离(cm) ≈ 时间(us) / 58
    ======================================================*/

    return time_us / 58U;
}
