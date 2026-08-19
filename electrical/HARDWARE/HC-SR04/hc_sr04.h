#ifndef __HC_SR04_H
#define __HC_SR04_H

#include "main.h" 

#define HC_SR04_TRIG_PORT GPIOA
#define HC_SR04_TRIG_PIN  GPIO_PIN_4

#define HC_SR04_ECHO_PORT GPIOA
#define HC_SR04_ECHO_PIN  GPIO_PIN_5

#include <stdint.h>
static void DWT_Delay_Init(void);
static void DWT_Delay_us(uint32_t us);
void HC_SR04_Init(void);
uint32_t HC_SR04_GetDistance(void);

#endif