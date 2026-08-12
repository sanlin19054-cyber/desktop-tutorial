#include "hc_sr04.h" 
#include "tim.h"

void HC_SR04_Init(void)
{
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
}

uint32_t HC_SR04_GetDistance(void)
{
    uint32_t time;

    // 發送10us高電位
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);

    // 讀取時間
    HAL_TIM_IC_Start(&htim4,TIM_CHANNEL_1);
	
    while(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_6)==GPIO_PIN_RESET);
    __HAL_TIM_SET_COUNTER(&htim4,0);

    while(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_6)==GPIO_PIN_SET);
    time = __HAL_TIM_GET_COUNTER(&htim4);

    HAL_TIM_IC_Stop(&htim4,TIM_CHANNEL_1);

    // 距離(cm)=時間(us)/58
    return time/58;
}