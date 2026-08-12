#ifndef __HC_SR04_H
#define __HC_SR04_H

#include "main.h" 

#define TRIG_GPIO_Port GPIOA
#define TRIG_Pin GPIO_PIN_8

void HC_SR04_Init(void);
uint32_t HC_SR04_GetDistance(void);

#endif