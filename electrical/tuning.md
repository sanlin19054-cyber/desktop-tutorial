# 智能救援小車項目調參與開發記錄

- **時間範圍**：2026年8月4日—2026年8月19日
- **記錄依據**：本次對話中已經進行的硬件、軟件、STM32、GitHub、傳感器、循跡、PID、避障、OLED、藍牙及樹莓派相關工作。
- **說明**：對話中明確出現「已測試/出現結果」的內容按實際結果記錄；只有代碼編寫、參數設計但沒有明確實車測試結果的部分，標記為「已編寫，待實測」，避免把計劃誤寫成測試結果。

---

## 2026年8月9日
STM32編譯環境與 Keil 工程排錯

### 工作內容
開始集中處理 STM32F103 小車工程 Car_stm32 的編譯環境問題。

### 遇到的問題
出現：
```
Target 'Car_stm32' uses ARM-Compiler
'Default Compiler Version 5'
which is not available.
```
之後又出現 CMSIS 相關錯誤，例如：
```
cmsis_version.h file not found
```
以及 `core_cm3.h`、`cmsis_version.h` 相關編譯問題。

### 調整內容
開始檢查：
- Keil ARM Compiler 版本
- 工程 Target 配置
- CMSIS 文件
- STM32F1 Standard Peripheral Library
- `core_cm3.c` / `core_cm3.h`

### 階段結果
確定工程存在明顯的編譯器版本與舊工程配置不匹配問題，為後面統一轉向 STM32 HAL 工程打下排錯基礎。

---

## 2026年8月10日
STM32 模組化編程開始

### 工作內容
開始將小車功能拆分為獨立的 `.c/.h` 模組，並建立較清晰的工程結構。

主要涉及：LED、USART、MOTOR、SERVO、HC-SR04、MPU6050、Avoidance、TRACK、PID、OLED、CAMERA

### 1. USART 模組

**編寫**：`USART.c` / `USART.h`

實現字串發送函數：
```c
USART_SendString(char *str)
```
核心使用：
```c
HAL_UART_Transmit(&huart1, ...);
```

**遇到問題**
```
use of undeclared identifier 'huart1'
```
後來確認原因是：CubeMX 的 `usart.c` 中定義了 `huart1`，`usart.h` 中需要：
```c
extern UART_HandleTypeDef huart1;
```
並在自己的 USART 模組中包含正確的 `usart.h`。

同時處理了 `USART.c` / `usart.c` 名稱衝突造成的 `USART.o → usart_1.o` 提示。

### 2. LED 模組

開始編寫：`led.c` / `led.h`

實現：
```c
LED_Init();
HAL_GPIO_WritePin();
HAL_GPIO_TogglePin();
```
期間出現：
```
implicit declaration of function 'LED_Init'
```
之後確認需要在 `main.c` 中正確 `#include "led.h"` 並把對應 `.c` 文件加入 Keil 工程。

### 階段結果

完成了從「單文件編程」向「功能模組化」的轉變。

---

## 2026年8月11日
MPU6050 模組與姿態計算

### 工作內容
開始編寫 MPU6050 模組。

建立：`MPU6050.c` / `MPU6050.h` / `mpu6050_angle.c` / `mpu6050_angle.h`

### 1. MPU6050.c / MPU6050.h

實現內容如下：
- MPU6050 初始化
- I²C 寄存器寫入
- I²C 寄存器讀取
- 三軸加速度讀取
- 三軸陀螺儀讀取

主要讀取數據：`Ax` `Ay` `Az` `Gx` `Gy` `Gz`

### 2. 地址設置

```c
#define MPU6050_ADDR (0x68 << 1)
```
明確 HAL I²C 使用的是左移一位後的地址。

### 3. 傳感器量程

當前代碼設置：
- 陀螺儀：±2000 °/s
- 加速度：±2 g

因此陀螺儀換算採用：`16.4 LSB/(°/s)`

### 4. 角度模組

開始編寫：`mpu6050_angle.c` / `mpu6050_angle.h`

利用加速度計算：`Pitch` `Roll`，並加入陀螺儀積分及互補濾波。

**遇到的問題**
```
use of undeclared identifier 'M_PI'
```
解決方案：
```c
#define M_PI 3.14159265358979323846
```
另外檢查了 `MPU6050_Angle_Update();` 的標頭文件聲明和 `main.c` 引用關係。

### 階段結果
MPU6050 已經從「原始 XYZ 數據讀取」進一步擴展到「姿態角計算」的算法層。

---

## 2026年8月12日
HC-SR04、避障及循環結構優化

### 工作內容
開始編寫超聲波模組及避障邏輯。

建立：`HC_SR04.c` / `HC_SR04.h` / `Avoidance.c` / `Avoidance.h`

### 1. HC-SR04

初始方案考慮：
```c
HAL_TIM_IC_Start(&htim4, TIM_CHANNEL_1);
```
利用 TIM4 輸入捕獲測量 ECHO。

**發現衝突**
後來結合已有 Timer 使用情況發現：TIM4 已經用於其他定時/控制需求，不適合再拿來專門做 HC-SR04 輸入捕獲。

因此改成：**DWT 微秒計時**
```
使用 DWT->CYCCNT 實現：
TRIG 10 μs
  ↓
等待 ECHO
  ↓
記錄 CPU cycle
  ↓
換算微秒
  ↓
距離 = 時間 / 58
```

### 2. 超時保護

加入：
```c
timeout = HAL_GetTick();
if ((HAL_GetTick() - timeout) > 50U)
{
    return 0;
}
```
目的：防止 HC-SR04 異常時程序一直卡在 `while(ECHO == GPIO_PIN_RESET)`。

### 3. 避障邏輯

最初思路：
```
發現障礙 → 停止 → 後退 → 轉向
```
隨後進一步優化為：
```
發現障礙
  ↓
停止
  ↓
後退
  ↓
隨機左/右轉
  ↓
前進脫離
  ↓
重新測距
```
並加入：不連續使用相同轉向方向，減少小車在障礙物附近原地打轉。

### 4. 文件結構優化

明確：
- `HC_SR04.c` → 只負責測距
- `MOTOR.c` → 只負責電機控制
- `Avoidance.c` → 只負責避障決策

### 階段結果
形成了較清晰的「傳感器層—決策層—執行層」架構。

---

## 2026年8月13日
循跡、PID、相機及系統整合

### 工作內容
開始完善循跡模組，並解決循跡轉彎後左右擺動的問題。

### 1. 四路循跡模組

建立：`track.c` / `track.h`

當前四路傳感器定義：
| 引腳 | 傳感器 |
|------|--------|
| PB0  | L1     |
| PB1  | L2     |
| PB10 | R2     |
| PB11 | R1     |

權重：
| L1 | L2 | R2 | R1 |
|----|----|----|----|
| -3 | -1 | +1 | +3 |

預設假設：黑線 = 0，白線 = 1

### 2. 原始循跡問題

實車表現回饋：小車轉彎後左右抖動。

分析為：
```
偏左 → 大幅修正 → 越過黑線 → 偏右 → 反向修正 → 再次越線
形成：左 → 右 → 左 → 右 的振盪
```

---

## 2026年8月13日 —— PID 加入循跡

### 修改內容
建立：`pid.c` / `pid.h`

PID 功能：
- **P** → 當前偏差修正
- **I** → 長期偏差消除
- **D** → 抑制快速變化和振盪

### 第一組建議參數
| 參數 | 值 |
|------|-----|
| Kp | 8 |
| Ki | 0 |
| Kd | 5 |

輸出限制：-35 ～ +35

### 循跡速度策略
| 情境 | base_speed |
|------|------------|
| 直線 | 40 |
| 小彎 | 35 |
| 大彎 | 30 |

並限制最終電機速度。

### 丟線策略
如果四個傳感器都是白色，則不立即停止，而是根據上一次有效誤差：
- 上次偏左 → 向左尋找
- 上次偏右 → 向右尋找

提高了重新找線的能力。

### 狀態
代碼結構已經完成，參數需要根據實際賽道繼續實車調試。

---

## 2026年8月13日
Camera Module 3 HDR 架構確認

### 硬體確認
確認使用：Raspberry Pi Camera Module 3 HDR

明確 Camera Module 3 不適合直接作為 STM32F103 的普通 UART/SPI 外設處理。

確定系統架構：
```
Camera Module 3
       ↓ CSI
Raspberry Pi
       ↓ 圖像識別
OpenCV / AI
       ↓ UART
STM32F103
       ↓
運動控制
```

### 軟體模組
因此 `CAMERA.c/.h` 不再負責直接驅動 IMX708，而設計成：STM32 與 Raspberry Pi 相機識別結果之間的通信接口。

例如定義：`PERSON` `OBSTACLE` `TARGET` 等結果信息。

### 階段結果
相機模組從「直接驅動攝像頭」的錯誤方向調整為「樹莓派視覺 + STM32 控制」的合理架構。
後來把這一塊刪掉了，所以hardwar裡沒有CAMERA.c/.h。

---

## 2026年8月14日
藍牙控制與 USART 接收

### 工作內容
開始把小車從「自主控制」擴展到「按鍵/藍牙控制」。

### JDY-31
確認使用：JDY-31

| 項目 | 設定 |
|------|------|
| 預設波特率 | 9600 bps |
| 串口格式 | 9600 / 8 / N / 1 |

STM32 USART1 設置：
- Baud Rate = 9600
- 8 data bits
- No parity
- 1 stop bit
- No hardware flow control
- TX + RX

AT 指令：記錄了 `AT+BAUD` 可用於查詢當前波特率。

### 按鍵控制協議
| 指令 | 功能 |
|------|------|
| 0x00 | 停止 |
| 0x01 | 前進 |
| 0x02 | 後退 |
| 0x03 | 左轉 |
| 0x04 | 右轉 |
| 0x05 | 避障模式 |
| 0x06 | 循跡模式 |
| 0x07 | 開燈 |
| 0x08 | 關燈 |

### STM32 接收機制
```c
uint8_t RxData;
HAL_UART_Receive_IT(&huart1, &RxData, 1);
HAL_UART_RxCpltCallback()
```
實現持續接收。

**遇到的問題**
```
function definition is not allowed here
```
原因是把 `HAL_UART_RxCpltCallback()` 錯誤地放進另一個函數內部。之後明確 `main()`、`HAL_UART_RxCpltCallback()`、`SystemClock_Config()`、`Error_Handler()` 必須是同級函數。

---

## 2026年8月14日
OLED 顯示模組

### 工作內容
增加 OLED：`oled.c` / `oled.h`

目標功能：只要能顯示字串即可。

### 初始設計
- I²C
- SSD1306
- 128×64

接口：
```c
OLED_Init();
OLED_Clear();
OLED_ShowString();
```

**編譯問題**
```
unknown type name 'uint8_t'
```
增加 `#include <stdint.h>` 後處理。

隨後出現 `HAL_I2C_Master_Transmit`、`hi2c1`、`HAL_Delay` 相關錯誤，確認需要正確包含：
```c
#include "main.h"
#include "i2c.h"
#include "stm32f1xx_hal.h"
```
並確認：
```c
extern I2C_HandleTypeDef hi2c1;
```

---

## 2026年8月14日
OLED 實機排查

### 實機狀態
燒錄後：OLED 無顯示。

### 已檢查
`main.c` 中已經存在：
```c
MX_I2C1_Init();
OLED_Init();
OLED_Clear();
OLED_ShowString(0, 0, "HELLO");
```
說明不是「完全沒調用 OLED」。

同時發現 `MX_I2C1_Init()` 被調用了兩次，因此計劃刪除重複調用。

### 硬體照片確認
OLED 背面看到：`GND` `VDD` `SCK` `SDA`，確認它是四線 I²C 模組。

### 當前排查方向
重點轉向：
- I²C 實際引腳
- OLED I²C 地址
- SSD1306 / SH1106 驅動晶片
- I²C 總線是否能掃描到設備

### 下一步測試方案
增加：
```c
HAL_I2C_IsDeviceReady()
```
掃描：`0x3C` `0x3D`，確認 OLED 實際地址。

### 狀態
OLED 目前不能記錄為「已經成功顯示」，只能記錄為「進入硬體地址與驅動兼容性排查階段」。

---

## 2026年8月15日
原理圖、硬體接口與模組映射繼續核對

### 工作內容
繼續檢查：
- STM32 原理圖
- GPIO 分配
- 電機接口
- OLED
- 傳感器
- 硬體接口名稱
- 元器件標註

同時繼續完善：電機驅動、傳感器接口、小車硬體模組化結構。

### 階段重點
發現舊工程和當前 HAL 工程存在一些 GPIO/Timer 映射差異，因此後續代碼必須以當前實際硬體接線為準，不能直接複製舊工程代碼。

---

## 2026年8月16日
工程命令與運行行為檢查

### 工作內容
繼續確認程序運行命令、燒錄行為和工程執行狀態，避免：
- 代碼正確
- 但運行配置錯誤
- 導致實際小車不工作

---

## 2026年8月17日
CubeMX 配置與項目基礎設施

### 工作內容
開始系統整理：STM32CubeMX、Keil、HAL、GPIO、TIM、USART、I2C 等基礎配置，形成與實際 PCB/原理圖對應的配置方案。

### 階段目標
逐步形成：
| 外設 | 用途 |
|------|------|
| TIM2 | 舵機 |
| TIM3 | 電機 PWM |
| I2C1 | MPU6050 / OLED |
| USART1 | JDY-31 / Raspberry Pi |
| GPIO | 循跡、超聲波、電機方向等 |

---


### 系統架構
```
                    main.c
                      │
        ┌─────────────┼─────────────┐
        ↓             ↓             ↓
      TRACK        AVOIDANCE      手動控制
        ↓             ↓             ↓
       PID        HC-SR04        USART/JDY-31
        ↓             ↓             ↓
        └────────── MOTOR ──────────┘
                       │
                       ↓
                    TB6612
                       │
                    電機
```
```
MPU6050 → Pitch / Roll → 安全保護
OLED → 狀態顯示
Camera Module 3 → Raspberry Pi → 視覺識別 → USART → STM32
```

### 當前主要參數記錄

#### 循跡 PID
| 參數 | 值 |
|------|-----|
| Kp | 8.0 |
| Ki | 0.0 |
| Kd | 5.0 |

- 輸出範圍：-35 ～ +35
- 循跡基礎速度：直線 40、小彎 35、大彎 30
- 最大速度：60

#### HC-SR04
| 項目 | 值 |
|------|-----|
| 安全閾值 | 15 cm |
| 避障判斷閾值 | 20 cm |
| 非常近 | 8 cm |
| 超時 | 50 ms |

距離換算：`距離(cm) ≈ Echo時間(us) / 58`

#### MPU6050
| 項目 | 值 |
|------|-----|
| 加速度 | ±2g |
| 陀螺儀 | ±2000°/s |
| Pitch 防傾翻閾值 | ±35° |
| Roll 防傾翻閾值 | ±35° |

#### 電機
| 動作 | PWM 值 |
|------|--------|
| 前進 | 50 / 50 |
| 後退 | -40 / -40 |
| 左轉 | -40 / 40 |
| 右轉 | 40 / -40 |

為了實際啟動，必要時提高 PWM 輸出。

#### JDY-31
| 項目 | 設定 |
|------|------|
| 波特率 | 9600 bps |
| 格式 | 8N1 |

控制協議：0x00 停止、0x01 前進、0x02 後退、0x03 左轉、0x04 右轉、0x05 避障、0x06 循跡、0x07 開燈、0x08 關燈

### 當前階段調參結論

截至 2026年8月19日，項目已經從單個模組測試進入：模組整合 → 參數調節 → 實車聯調。




尤其是之前已經出現過的**「轉彎後左右抖動」**，當前對應的調參方向已經從固定左右修正升級到了 加權誤差 + PD/PID + 轉彎降速 + 丟線恢復；後續應該重點通過實車測試記錄 Kp/Kd/基礎速度 的變化，而不是一次同時修改多個參數。

> 建議從現在開始，每次只修改一組參數，並記錄「修改前 → 修改後 → 實車表現」，這樣後續才能真正得到一份可複現的調參曲線。
