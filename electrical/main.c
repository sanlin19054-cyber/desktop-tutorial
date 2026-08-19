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
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "led.h"
#include "my_usart.h" 
#include "motor.h"
#include "servo.h"
#include "mpu6050.h" 
#include "pid.h" 
#include "hc_sr04.h" 
#include "mpu6050_angle.h" 
#include "avoidance.h"
#include "track.h"
#include "oled.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* MPU6050 防翻車角度 */
#define MAX_PITCH_ANGLE     35.0f
#define MAX_ROLL_ANGLE      35.0f

/* HC-SR04 緊急停止距離 */
#define SAFE_DISTANCE       15U

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* RxBuf: HAL 接收緩衝區（non-volatile，避免對 HAL API 傳 volatile* 造成警告）
   RxData: 主循環實際讀取的最新指令拷貝（volatile，保證中斷更新對主循環可見） */
static uint8_t RxBuf;
volatile uint8_t RxData;

float pitch = 0.0f;
float roll  = 0.0f;

uint32_t distance = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void Sensor_Update(void);
static void Safety_Check(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*======================================================
  感測器數據更新

  更新 MPU6050 姿態角與前方超聲波測距
======================================================*/
static void Sensor_Update(void)
{
    MPU6050_Angle_Update();
    pitch = MPU6050_GetPitch();
    roll  = MPU6050_GetRoll();

    SERVO_SetAngle(90);
    HAL_Delay(30);
    distance = HC_SR04_GetDistance();
}

/*======================================================
  安全檢查

  角度過大（防翻車）或前方障礙物過近時停止電機
======================================================*/
static void Safety_Check(void)
{
    if ((pitch >  MAX_PITCH_ANGLE) || (pitch < -MAX_PITCH_ANGLE) ||
        (roll  >  MAX_ROLL_ANGLE)  || (roll  < -MAX_ROLL_ANGLE))
    {
        Motor_Stop();
        return;
    }

    if ((distance > 0U) && (distance < SAFE_DISTANCE))
    {
        Motor_Stop();
    }
}

/*======================================================
  把 1 byte 轉 2 位十六進制字符串（用於 OLED 調試顯示）
======================================================*/
static void ByteToHexStr(uint8_t val, char out[3])
{
    static const char hex[] = "0123456789ABCDEF";
    out[0] = hex[(val >> 4) & 0x0F];
    out[1] = hex[ val       & 0x0F];
    out[2] = '\0';
}

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

  /* ===== 故障診斷：HAL_Init 後立即閃 LED 3 次（完全不依賴任何外設） =====
     直接操作暫存器設定 PC13 為推挽輸出（不依賴 MX_GPIO_Init）。
     如果這 3 次閃爍看不到 → STM32 沒啟動 / 晶片沒供電 / 燒錄失敗 / boot0 接錯
     如果看到 3 次閃爍但後面卡住 → 卡在某個 MX_XXX_Init */
  {
      volatile uint32_t i;
      /* 啟用 GPIOC 時鐘（RCC_APB2ENR bit4 = IOPCEN） */
      RCC->APB2ENR |= (1U << 4);
      /* 設定 PC13 為推挽輸出，2MHz：CRH 的 mode13=2, cnf13=0 */
      GPIOC->CRH &= ~(0xFU << 20);     /* 清除 PC13 對應位 */
      GPIOC->CRH |=  (0x2U << 20);     /* mode=2 (2MHz output), cnf=0 (push-pull) */

      for (int k = 0; k < 3; k++) {
          GPIOC->BSRR = (1U << 13);            /* 滅（PC13=1，LED 滅）*/
          for (i = 0; i < 300000; i++);
          GPIOC->BRR  = (1U << 13);            /* 亮（PC13=0，LED 亮）*/
          for (i = 0; i < 300000; i++);
      }
      GPIOC->BSRR = (1U << 13);                /* 保持滅 */
  }
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* ===== 故障診斷：MX_xxx_Init 全部完成後閃 1 次（長亮 1 秒）=====
     如果看不到這次 → 卡在某個 MX_xxx_Init（GPIO/I2C/TIM/UART） */
  {
      volatile uint32_t i;
      GPIOC->BRR  = (1U << 13);            /* 亮 */
      for (i = 0; i < 1000000; i++);
      GPIOC->BSRR = (1U << 13);            /* 滅 */
  }

	 /*======================================================
      初始化
    ======================================================*/

	LED_Init();
	Motor_Init();
	MX_TIM2_Init();
	SERVO_Init();
	TRACK_Init();
	MPU6050_Init();
	HC_SR04_Init();

  /* ===== 故障診斷：硬體模組全部 Init 完成後閃 2 次（短亮）=====
     如果看不到這 2 次閃爍 → 卡在某個模組 Init（MPU6050/HC_SR04 最可能） */
  {
      volatile uint32_t i;
      for (int k = 0; k < 2; k++) {
          GPIOC->BRR  = (1U << 13);            /* 亮 */
          for (i = 0; i < 300000; i++);
          GPIOC->BSRR = (1U << 13);            /* 滅 */
          for (i = 0; i < 300000; i++);
      }
  }

	OLED_Init();
    /*
     * 舵機回到中間
     */
    SERVO_SetAngle(90);
	Avoidance_Init();

    /*
     * 初始停止電機
     */
    Motor_Stop();

    /*
     * 等待系統穩定
     */
    HAL_Delay(500);

    /*
     * 啟動 UART1 中斷接收（遠端遙控指令）
     * 對 HAL 傳 non-volatile 的 RxBuf；在回調中拷貝到 volatile RxData 供主循環使用
     */
    HAL_UART_Receive_IT(&huart1, &RxBuf, 1);

    /* 開機顯示一次即可，避免循環內反覆擦屏(I2C阻塞)拖慢指令響應 */
    OLED_Clear();
    OLED_ShowString(0, 0, "HELLO");
    OLED_ShowString(0, 16, "STM32 OK");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
	while (1)
  {
        uint8_t raw, cmd;
        char hex[3];
        char line[16];

        /* ---------- 1. 讀取原始接收字節，同時支持 HEX(0x01) 與 ASCII('1') ---------- */
        raw = (uint8_t)RxData;       /* volatile 只讀一次 */
        cmd = raw;

        /* 兼容：手機App常見發 ASCII '0'~'8'，自動映射到 0x00~0x08 */
        if (raw >= '0' && raw <= '8')
        {
            cmd = raw - '0';
        }

        /* ---------- 2. OLED 顯示調試信息（確認是否真的收到了字節） ---------- */
        OLED_Clear();
        OLED_ShowString(0, 0, "HELLO STM32");

        ByteToHexStr(raw, hex);
        line[0] = 'R'; line[1] = 'X'; line[2] = ':';
        line[3] = hex[0]; line[4] = hex[1]; line[5] = '\0';
        OLED_ShowString(0, 16, line);

        ByteToHexStr(cmd, hex);
        line[0] = 'C'; line[1] = 'M'; line[2] = 'D'; line[3] = ':';
        line[4] = hex[0]; line[5] = hex[1]; line[6] = '\0';
        OLED_ShowString(0, 32, line);

        /* ---------- 3. 執行指令 ---------- */
		switch(cmd)
				{
        case 0x00:
            Motor_Stop();
            break;

        case 0x01:
            Motor_Control(50, 50);
            break;

        case 0x02:
            Motor_Control(-40, -40);
            break;

        case 0x03:
            Motor_Control(-40, 40);
            break;

        case 0x04:
            Motor_Control(40, -40);
            break;

        case 0x05:
        {
            /* 避障模式：舵機掃描前/左/右三個方向測距 */
            uint32_t head, left, right;

            SERVO_SetAngle(90);
            HAL_Delay(200);
            head = HC_SR04_GetDistance();

            SERVO_SetAngle(180);
            HAL_Delay(200);
            left = HC_SR04_GetDistance();

            SERVO_SetAngle(0);
            HAL_Delay(200);
            right = HC_SR04_GetDistance();

            SERVO_SetAngle(90);
            HAL_Delay(200);

            Avoidance_Run(head, left, right);
            break;
        }

        case 0x06:
            /* 循跡模式 */
            TRACK_Run();
            break;

				case 0x07:
            /* 開燈 */
            LED_ON();
            break;

				case 0x08:
            /* 關燈 */
            LED_OFF();
            break;

        default:
            /* 未知字節：保持上次狀態，不要強制停車，
               否則波特率/格式不匹配時永遠表現為「無反應」 */
            break;
    }

    /* ---------- 4. 調試階段：先跳過 Safety_Check 排除干擾
       (確認車能動後再打開 Sensor_Update + Safety_Check) ---------- */
#if 0
    Sensor_Update();
    Safety_Check();
#endif

        /* 顯示刷新需要時間，不用再加 delay；確保整體循環足夠快 */
        HAL_Delay(50);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/*======================================================
  UART 接收完成回調

  每收到 1 byte 遙控指令後自動重新啟動接收，
  使 RxData 持續更新最新的指令值。
======================================================*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* 把 HAL 收到的字節拷貝到 volatile RxData，
           再重啟中斷接收，這樣主循環讀到的一定是最新指令 */
        RxData = RxBuf;
        HAL_UART_Receive_IT(&huart1, &RxBuf, 1);
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
		/*
     * 發生嚴重錯誤時停止
     */
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
