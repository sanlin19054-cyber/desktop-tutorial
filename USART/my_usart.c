#include "USART.h" 
#include "usart.h"

//這是 HAL 的 UART 發送函數，發送指定字元
void USART_SendString(char *str)
{
	 while(*str)
    {
        HAL_UART_Transmit(&huart1,
                          (uint8_t *)str,
                          1,
                          100);

        str++;
		}
}
