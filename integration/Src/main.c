/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "pi_protocol.h"
#include "stp23l.h"
#include "motor_control.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint32_t telemetry_tick = 0;
static PiProtocol pi_protocol;
static uint8_t pi_motor_command_active = 0U;
static uint8_t pi_servo_command_active = 0U;
static uint8_t servo_enabled_mask = 0U;
static Stp23l stp_left;
static Stp23l stp_right;

#define STP23L_TELEMETRY_MAX_AGE_MS 300U

/* ST-LINK debugger watch variables. 0xFFFF means invalid or stale. */
volatile uint16_t stp_left_distance_mm = STP23L_INVALID_DISTANCE_MM;
volatile uint16_t stp_right_distance_mm = STP23L_INVALID_DISTANCE_MM;
volatile uint8_t stp_distance_valid_mask = 0U;
volatile uint8_t stp_left_confidence = 0U;
volatile uint8_t stp_right_confidence = 0U;
volatile uint32_t stp_left_frame_count = 0U;
volatile uint32_t stp_right_frame_count = 0U;
volatile uint32_t stp_left_invalid_frame_count = 0U;
volatile uint32_t stp_right_invalid_frame_count = 0U;
volatile uint32_t stp_left_uart_error_count = 0U;
volatile uint32_t stp_right_uart_error_count = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define SERVO_CAMERA_MASK    0x01U
#define SERVO_LEFT_ARM_MASK  0x02U
#define SERVO_RIGHT_ARM_MASK 0x04U

static void Servo_StopAll(void)
{
    (void)HAL_TIM_PWM_Stop(&htim9, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Stop(&htim9, TIM_CHANNEL_2);
    (void)HAL_TIM_PWM_Stop(&htim10, TIM_CHANNEL_1);
    servo_enabled_mask = 0U;
    pi_servo_command_active = 0U;
}

static uint8_t Servo_StartChannel(TIM_HandleTypeDef *timer, uint32_t channel)
{
    return (HAL_TIM_PWM_Start(timer, channel) == HAL_OK) ? 1U : 0U;
}

static void Servo_Apply(const PiServoCommand *command)
{
    uint8_t requested;

    if (command == NULL)
    {
        return;
    }

    requested = command->enable_mask & 0x07U;
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, command->camera_us);
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_2, command->left_arm_us);
    __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, command->right_arm_us);

    if ((requested & SERVO_CAMERA_MASK) != 0U)
    {
        if (
            ((servo_enabled_mask & SERVO_CAMERA_MASK) == 0U) &&
            (Servo_StartChannel(&htim9, TIM_CHANNEL_1) == 0U)
        )
        {
            Servo_StopAll();
            return;
        }
    }
    else if ((servo_enabled_mask & SERVO_CAMERA_MASK) != 0U)
    {
        (void)HAL_TIM_PWM_Stop(&htim9, TIM_CHANNEL_1);
    }

    if ((requested & SERVO_LEFT_ARM_MASK) != 0U)
    {
        if (
            ((servo_enabled_mask & SERVO_LEFT_ARM_MASK) == 0U) &&
            (Servo_StartChannel(&htim9, TIM_CHANNEL_2) == 0U)
        )
        {
            Servo_StopAll();
            return;
        }
    }
    else if ((servo_enabled_mask & SERVO_LEFT_ARM_MASK) != 0U)
    {
        (void)HAL_TIM_PWM_Stop(&htim9, TIM_CHANNEL_2);
    }

    if ((requested & SERVO_RIGHT_ARM_MASK) != 0U)
    {
        if (
            ((servo_enabled_mask & SERVO_RIGHT_ARM_MASK) == 0U) &&
            (Servo_StartChannel(&htim10, TIM_CHANNEL_1) == 0U)
        )
        {
            Servo_StopAll();
            return;
        }
    }
    else if ((servo_enabled_mask & SERVO_RIGHT_ARM_MASK) != 0U)
    {
        (void)HAL_TIM_PWM_Stop(&htim10, TIM_CHANNEL_1);
    }

    servo_enabled_mask = requested;
    pi_servo_command_active = (requested != 0U) ? 1U : 0U;
}


/* Encoder debug values visible in the debugger. */
volatile int32_t encoder_delta_1 = 0;
volatile int32_t encoder_delta_2 = 0;
volatile int32_t encoder_delta_3 = 0;

static uint32_t encoder_sample_tick = 0;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM8_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_TIM9_Init();
  MX_TIM10_Init();
  /* USER CODE BEGIN 2 */
  if (MotorControl_Init() != HAL_OK)
  {
      Error_Handler();
  }

  /* Preload safe servo neutral positions, but keep the outputs disabled. */
  __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 1500U);
  __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_2, 1500U);
  __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 1500U);
  Servo_StopAll();

  encoder_sample_tick = HAL_GetTick();
  PiProtocol_Init(&pi_protocol, &huart1);
  if (PiProtocol_Start(&pi_protocol) != HAL_OK)
  {
      Error_Handler();
  }
  Stp23l_Init(&stp_left, &huart2);
  Stp23l_Init(&stp_right, &huart3);
  if (Stp23l_Start(&stp_left) != HAL_OK)
  {
      Error_Handler();
  }
  if (Stp23l_Start(&stp_right) != HAL_OK)
  {
      Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  {
	      uint32_t now_ms = HAL_GetTick();
	      uint8_t valid_mask = 0U;
	      uint16_t left_distance = STP23L_INVALID_DISTANCE_MM;
	      uint16_t right_distance = STP23L_INVALID_DISTANCE_MM;

	      Stp23l_Service(&stp_left);
	      Stp23l_Service(&stp_right);
	      if (
	          Stp23l_GetDistance(
	              &stp_left,
	              now_ms,
	              STP23L_TELEMETRY_MAX_AGE_MS,
	              &left_distance
	          ) != 0U
	      )
	      {
	          valid_mask |= 0x01U;
	      }
	      if (
	          Stp23l_GetDistance(
	              &stp_right,
	              now_ms,
	              STP23L_TELEMETRY_MAX_AGE_MS,
	              &right_distance
	          ) != 0U
	      )
	      {
	          valid_mask |= 0x02U;
	      }
	      stp_left_distance_mm = left_distance;
	      stp_right_distance_mm = right_distance;
	      stp_distance_valid_mask = valid_mask;
	      stp_left_confidence = stp_left.confidence;
	      stp_right_confidence = stp_right.confidence;
	      stp_left_frame_count = stp_left.frame_count;
	      stp_right_frame_count = stp_right.frame_count;
	      stp_left_invalid_frame_count = stp_left.invalid_frame_count;
	      stp_right_invalid_frame_count = stp_right.invalid_frame_count;
	      stp_left_uart_error_count = stp_left.uart_error_count;
	      stp_right_uart_error_count = stp_right.uart_error_count;
	  }
	  if ((HAL_GetTick() - encoder_sample_tick) >= 10)
	  	  {
	          MotorEncoderDeltas deltas;
	  	      encoder_sample_tick = HAL_GetTick();
	  	      MotorControl_SampleEncoders();
	          deltas = MotorControl_GetEncoderDeltas();
	          encoder_delta_1 = deltas.left_front;
	          encoder_delta_2 = deltas.right_front;
	          encoder_delta_3 = deltas.rear;
	  	  }
      {
          PiMotorCommand command;
          if (PiProtocol_TakeMotorCommand(&pi_protocol, &command) != 0U)
          {
              MotorControl_SetAll(
                  command.motor_1,
                  command.motor_2,
                  command.motor_3
              );
              pi_motor_command_active =
                  ((command.motor_1 != 0) ||
                   (command.motor_2 != 0) ||
                   (command.motor_3 != 0)) ? 1U : 0U;
          }

          if (
              (pi_motor_command_active != 0U) &&
              (PiProtocol_IsMotorCommandExpired(
                  &pi_protocol,
                  HAL_GetTick()
              ) != 0U)
          )
          {
              MotorControl_StopAll();
              pi_motor_command_active = 0U;
          }
      }

      {
          PiServoCommand command;
          if (PiProtocol_TakeServoCommand(&pi_protocol, &command) != 0U)
          {
              Servo_Apply(&command);
          }

          if (
              (pi_servo_command_active != 0U) &&
              (PiProtocol_IsServoCommandExpired(
                  &pi_protocol,
                  HAL_GetTick()
              ) != 0U)
          )
          {
              Servo_StopAll();
          }
      }
	  if ((HAL_GetTick() - telemetry_tick) >= 100)
	  {
	      telemetry_tick = HAL_GetTick();
          (void)PiProtocol_SendEncoderTelemetry(
              &pi_protocol,
              encoder_delta_1,
              encoder_delta_2,
              encoder_delta_3
          );
          (void)PiProtocol_SendDistanceTelemetry(
              &pi_protocol,
              stp_left_distance_mm,
              stp_right_distance_mm,
              stp_distance_valid_mask
          );
	  }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        PiProtocol_OnRxComplete(&pi_protocol, huart);
    }
    else
    {
        Stp23l_OnRxComplete(&stp_left, huart);
        Stp23l_OnRxComplete(&stp_right, huart);
    }
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    Stp23l_OnRxHalfComplete(&stp_left, huart);
    Stp23l_OnRxHalfComplete(&stp_right, huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        PiProtocol_OnUartError(&pi_protocol, huart);
    }
    else
    {
        Stp23l_OnUartError(&stp_left, huart);
        Stp23l_OnUartError(&stp_right, huart);
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
