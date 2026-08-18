#include "stm32f10x.h"
#include "Delay.h"
#include "LED.h"
#include "TIM3_PWM.h"
#include "MOTOR.h"
#include "OLED.h"
#include "USART1.h"
#include "Servo.h"
#include "HCSR04.h"
#include "mycontrol.h"
#include "Trace.h"
#include "sys.h"

extern u8 RxData;
uint16_t HCSR04_Distance = 0;

static u8 mappedCmd = 0x00;

static void Map_RxData_To_Command(u8 raw)
{
	if (raw >= '0' && raw <= '8')
	{
		mappedCmd = raw - '0';
	}
	else if (raw <= 0x08)
	{
		mappedCmd = raw;
	}
	else
	{
		mappedCmd = 0x00;
	}
}

static void ByteToHexStr(u8 byte, char *buf)
{
	const char hex[] = "0123456789ABCDEF";
	buf[0] = hex[(byte >> 4) & 0x0F];
	buf[1] = hex[byte & 0x0F];
	buf[2] = '\0';
}

static void OLED_ShowHex(u8 x, u8 y, u8 byte, u8 size)
{
	char buf[4];
	ByteToHexStr(byte, buf);
	OLED_ShowString(x, y, (u8 *)buf, size, 1);
}

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	TIM3_PWM_Init(7199, 1);
	MOTOR_GPIO_Init();
	LED_Init();

	LED_Open();
	Delay_ms(200);
	LED_Close();
	Delay_ms(200);

	OLED_Init();
	OLED_ColorTurn(0);
	OLED_DisplayTurn(0);
	OLED_Clear();
	OLED_Refresh();
	OLED_ShowString(0, 0, (u8*)"STM32 SmartCar", 16, 1);
	OLED_ShowString(0, 16, (u8*)"RX:   CMD:", 16, 1);
	OLED_ShowString(0, 32, (u8*)"Dist:   cm", 16, 1);
	OLED_ShowString(0, 48, (u8*)"Car v2.0 SPL", 16, 1);
	OLED_Refresh();

	USART1_Init(9600);
	myServo_Init(1999, 719);
	Servo_SetAngle(90);
	HCSR04_Init();
	Trace_Init();

	LED_Open();
	Delay_ms(200);
	LED_Close();
	Delay_ms(200);
	LED_Open();
	Delay_ms(200);
	LED_Close();

	while (1)
	{
		Map_RxData_To_Command(RxData);

		switch (mappedCmd)
		{
			case 0x00:
				Set_Car_Speed(0, 0);
				HCSR04_Distance = HCSR04_GetValue();
				break;

			case 0x01:
				Set_Car_Speed(5500, 5500);
				break;

			case 0x02:
				Set_Car_Speed(-5500, -5500);
				break;

			case 0x03:
				Set_Car_Speed(-3800, 3800);
				break;

			case 0x04:
				Set_Car_Speed(3800, -3800);
				break;

			case 0x05:
				Avoidance();
				break;

			case 0x06:
				Trace_task();
				break;

			case 0x07:
				LED_Open();
				break;

			case 0x08:
				LED_Close();
				break;

			default:
				Set_Car_Speed(0, 0);
				break;
		}

		OLED_ShowHex(40, 16, RxData, 16);
		OLED_ShowHex(104, 16, mappedCmd, 16);
		OLED_ShowNum(40, 32, HCSR04_Distance, 3, 16, 1);

		OLED_Refresh();
	}
}
