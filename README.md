# 大学生智能小车综合设计方案

> **涵盖赛事**：大学生电子设计大赛（循迹小车赛道）、大学生工程实践与创新大赛（智能救援赛道）
> **文档版本**：v1.0
> **更新日期**：2026年8月30日
> 說明：本倉庫僅供學習參考，很多文件沒有上傳（其實因為我不知道怎麼傳體積過大的文件夾）

---

## 目录

- [第一部分 电赛循迹遥控小车](#第一部分-电赛循迹遥控小车)
  - [1.1 项目概述](#11-项目概述)
  - [1.2 功能需求分析](#12-功能需求分析)
  - [1.3 系统总体架构](#13-系统总体架构)
  - [1.4 硬件设计方案](#14-硬件设计方案)
  - [1.5 软件设计方案](#15-软件设计方案)
  - [1.6 Android APP 设计](#16-android-app-设计)
  - [1.7 关键算法实现](#17-关键算法实现)
  - [1.8 测试与性能指标](#18-测试与性能指标)
- [第二部分 智能救援小车](#第二部分-智能救援小车)
  - [2.1 项目概述](#21-项目概述)
  - [2.2 功能需求分析](#22-功能需求分析)
  - [2.3 系统总体架构](#23-系统总体架构)
  - [2.4 硬件设计方案](#24-硬件设计方案)
  - [2.5 软件设计方案](#25-软件设计方案)
  - [2.6 多传感器融合与目标识别](#26-多传感器融合与目标识别)
  - [2.7 运动控制与越障策略](#27-运动控制与越障策略)
  - [2.8 安全保护机制](#28-安全保护机制)
  - [2.9 测试与性能指标](#29-测试与性能指标)
- [两方案对比总结](#两方案对比总结)
- [参考文献](#参考文献)

---

# 第一部分 电赛循迹遥控小车

## 1.1 项目概述

本设计面向**全国大学生电子设计大赛（NUEDC）循迹小车赛道**，设计并实现一款集**Android 远程遥控**与**自主循迹避障**于一体的多功能智能小车系统。系统以 STM32F103 单片机为主控核心，通过 WiFi/Bluetooth 模块与 Android 移动端 APP 通信，实现远程操控；同时搭载红外循迹传感器与超声波避障模块，具备自主导航能力。

### 1.1.1 设计目标

| 指标项 | 目标参数 |
|--------|----------|
| 遥控距离（蓝牙） | ≥ 10 m |
| 遥控距离（WiFi） | ≥ 50 m |
| 循迹速度 | 0.5 ~ 2.0 m/s（可调） |
| 循迹精度 | 偏差 ≤ 2 cm |
| 避障检测距离 | 2 ~ 40 cm |
| 最小转弯半径 | ≤ 15 cm |
| 连续续航时间 | ≥ 2 小时 |

---

## 1.2 功能需求分析

### 1.2.1 功能清单

#### （1）Android 远程遥控功能
  **车辆启停**：APP 端一键启停电机电源
  **灯光控制**：前大灯开/关、转向灯（左/右闪）、刹车灯控制
  **运动控制**：
    前进 / 后退（PWM 调速，支持 0~100% 占空比调节）
    左转 / 右转（差速转向 / 舵机转向，支持角度调节）
    紧急刹车
  **模式切换**：手动遥控模式 ↔ 自动循迹模式

#### （2）自主循迹功能
  支持黑色赛道 / 白色背景或白色赛道 / 黑色背景识别
  支持直道、弯道（≥90°）、S 弯、十字交叉路口识别
  循迹过程中自动调速（弯道减速、直道加速）
  丢失赛道线时自动停车并报警

#### （3）避障功能
  循迹模式下前方障碍物检测
  障碍物距离 < 阈值时自动减速 / 停车
  支持简单绕行策略

---

## 1.3 系统总体架构

```
┌──────────────────────────────────────────────────────────────────┐
│                     Android 移动端 APP                            │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐      │
│  │ 方向摇杆  │  │ 灯光按钮 │   │ 调速滑条 │  │ 模式/状态显示 │      │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬───────┘      │
└───────┼─────────────┼─────────────┼────────────────┼────────────┘
        │             │             │                │
        ▼             ▼             ▼                ▼
┌──────────────────────────────────────────────────────────────────┐
│              通信层（Bluetooth HC-05 / ESP8266 WiFi）             │
└─────────────────────────────────┬────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────┐
│                    主控层 STM32F103C8T6                           │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌──────────────┐   │
│  │ 通信协议解析│ │ 运动控制PID │ │ 循迹处理    │ │ 避障决策逻辑  │   │
│  └──────┬─────┘ └──────┬─────┘ └──────┬─────┘ └──────┬───────┘    │
└─────────┼───────────────┼───────────────┼───────────────┼──────────┘
          │               │               │               │
          ▼               ▼               ▼               ▼
┌──────────────────────────────────────────────────────────────────┐
│                       执行与感知层                                │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌──────────────┐   │
│  │电机驱动L298N│ │ LED灯光组  │ │红外循迹模块 │ │超声波HC-SR04  │   │
│  └────────────┘ └────────────┘ └────────────┘ └──────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

---

## 1.4 硬件设计方案

### 1.4.1 主控单元

| 器件型号 | 功能说明 | 数量 |
|----------|----------|------|
| STM32F103C8T6 | 主控 MCU，72MHz，64KB Flash | 1 |
| AMS1117-3.3 | 3.3V 稳压芯片 | 1 |
| 8MHz 晶振 | 外部高速时钟 | 1 |
| CH340G | USB 转串口（程序下载与调试） | 1 |

**最小系统电路要点**：
  NRST 引脚接 10kΩ 上拉电阻 + 100nF 去耦电容
  BOOT0 接下拉电阻（默认从 Flash 启动）
  VDDA/VSSA 模拟电源独立滤波

### 1.4.2 动力驱动系统

| 器件型号 | 功能说明 | 数量 |
|----------|----------|------|
| L298N 双 H 桥电机驱动板 | 驱动两路直流减速电机 | 1 |
| JGB37-520 直流减速电机（带编码器可选） | 12V，转速 300RPM | 2~4 |
| 12V 锂电池组（18650 3S） | 动力电源，2200mAh | 1 |

**接线说明**：
  L298N IN1~IN4 → STM32 PB12~PB15（方向控制）
  L298N ENA/ENB → STM32 PA0/PA1（PWM 调速，TIM2_CH1/CH2）
  ENA/ENB 跳线帽拔出，改用 PWM 控制

### 1.4.3 循迹传感模块

采用 **8 路红外循迹传感器阵列 TCRT5000**：

| 特性 | 参数 |
|------|------|
| 检测距离 | 1~8 mm（建议安装高度 5mm） |
| 传感器间距 | 15 mm |
| 输出 | 数字量（LM393 比较器） |
| 灵敏度调节 | 电位器可调 |

**接线方式**：
  D0~D7 → STM32 PC0~PC7（8 位并行读取）
  建议使用 8 位端口一次性读取，提高效率

### 1.4.4 避障传感模块

| 器件 | 型号 | 数量 | 接口 |
|------|------|------|------|
| 超声波测距 | HC-SR04 | 2（前/左） | Trig→PB3, Echo→PB4（前）；Trig→PB5, Echo→PB6（左） |
| 红外避障（备选） | E18-D80NK | 2 | PB8, PB9 |

### 1.4.5 灯光系统

| 灯光类型 | GPIO | 说明 |
|----------|------|------|
| 左前大灯 | PA8 | 高电平点亮，PWM 可调亮度 |
| 右前大灯 | PA9 | 同上 |
| 左转向灯 | PA10 | 闪烁控制（500ms 周期） |
| 右转向灯 | PA11 | 同上 |
| 刹车灯 | PA12 | 制动时点亮 |
| 状态指示灯（板载） | PC13 | 运行/故障指示 |

> 注：LED 串联 220Ω~1kΩ 限流电阻，高功率灯光通过三极管/MOS 管扩流。

### 1.4.6 通信模块（二选一或两者兼具）

**方案 A：蓝牙 HC-05**
  接口：USART1（TX→PA10, RX→PA9，注意与灯光引脚复用需重映射）
  波特率：9600，8N1
  配对密码：默认 1234

**方案 B：WiFi ESP8266**
  接口：USART2（TX→PA3, RX→PA2）
  工作模式：AP 模式（手机直接连接）或 STA 模式（连接路由器）
  协议：TCP Server，端口 8080

### 1.4.7 电源管理

```
12V 电池 ──┬──→ L298N VMOT（电机电源）
           │
           └──→ 5V 降压模块(LM2596) ──┬──→ L298N VCC
                                      │
                                      └──→ AMS1117-3.3V ──→ STM32/传感器
```

---

## 1.5 软件设计方案

### 1.5.1 软件开发环境

  **IDE**：Keil MDK-ARM 5.38
  **固件库**：STM32 Standard Peripheral Library 3.6.0 / HAL Library 1.8.x
  **编译工具**：ARMCC V5 / ARMCLANG V6
  **调试工具**：ST-Link V2

### 1.5.2 软件总体流程

```
┌─────────┐
│ 系统上电 │
└────┬────┘
     ▼
┌───────────────────────────┐
│ 初始化：GPIO/TIM/UART/NVIC│
└────┬──────────────────────┘
     ▼
┌───────────────────────────┐      ┌──────────────────────┐
│  模式判定：APP 指令?      │─────→│ 遥控模式处理流程     │
└────┬──────────────────────┘ 否  └──────────────────────┘
     │是
     ▼
┌───────────────────────────┐
│  循迹+避障主循环          │←────────────┐
│  ┌──────────┐ ┌────────┐ │             │
│  │读取循迹值 │ │避障测距 │ │             │
│  └────┬─────┘ └───┬────┘ │             │
│       ▼           ▼      │             │
│  ┌────────────────────┐  │             │
│  │   决策：停车/调速/  │  │             │
│  │   转向/避障绕行     │  │             │
│  └────────┬───────────┘  │             │
│           ▼              │             │
│  ┌────────────────────┐  │             │
│  │  输出 PWM+方向控制  │──┘             │
│  └────────────────────┘                │
│     5ms 周期                           │
└────────────────────────────────────────┘
```

### 1.5.3 通信协议设计

采用**帧头 + 命令字 + 数据长度 + 数据 + 校验 + 帧尾**格式：

| 字段 | 长度（字节） | 说明 |
|------|-------------|------|
| Frame Header | 2 | 固定 `0xAA 0x55` |
| CMD | 1 | 命令字（见下表） |
| LEN | 1 | 数据区字节数 |
| DATA | LEN | 有效载荷 |
| CHECKSUM | 1 | CMD+LEN+DATA 累加和低 8 位 |
| Frame Tail | 2 | 固定 `0x55 0xAA` |

**命令字定义**：

| CMD | 名称 | DATA 内容 | 方向 |
|-----|------|-----------|------|
| 0x01 | 运动控制 | [0]:方向(0停/1前/2后/3左/4右) [1]:速度PWM(0~100) | APP→车 |
| 0x02 | 灯光控制 | [0]:灯光位掩码(b0大灯/b1左闪/b2右闪/b3刹车) | APP→车 |
| 0x03 | 模式切换 | [0]:0=循迹模式 1=遥控模式 | APP→车 |
| 0x10 | 状态上报 | [0]:模式 [1]:速度 [2-3]:电压(mV高/低) [4]:障碍距离 | 车→APP |
| 0x80 | 心跳帧 | 无 | 双向 |

---

## 1.6 Android APP 设计

### 1.6.1 开发环境

  **IDE**：Android Studio Ladybug | 2024.2.1
  **语言**：Kotlin / Java
  **最低 SDK**：API 24 (Android 7.0)
  **UI 框架**：Jetpack Compose / 传统 View 系统

### 1.6.2 APP 功能模块架构

```
┌─────────────────────────────────────────────────┐
│                 MainActivity                    │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐   │
│  │ 连接页面   │ │ 控制页面   │ │ 状态页面     │   │
│  └─────┬──────┘ └─────┬──────┘ └─────┬──────┘   │
└────────┼───────────────┼───────────────┼────────┘
         │               │               │
         ▼               ▼               ▼
┌─────────────────────────────────────────────────┐
│           通信服务层（BluetoothService）         │
│  ┌─────────────┐ ┌──────────────┐ ┌──────────┐  │
│  │ 设备扫描/   │ │  数据发送    │ │ 数据接收   │  │
│  │ 配对连接    │ │  （协议组帧）│ │（解帧解析） │  │
│  └─────────────┘ └──────────────┘ └──────────┘  │
└─────────────────────────────────────────────────┘
```

### 1.6.3 控制页面 UI 布局

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  尚未選擇設備                 已發送次數              上次發送       未連接     │
│  ─────────                    0                       ()          [連接設備]  │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ← RX：等待接收...                                                            │
│                                                                              │
│  模式切換                                                                     │
│                                                                              │
│  ┌──────────────────────────────┐    ┌──────────────────────────────┐        │
│  │                              │    │                              │        │
│  │          避障                │    │          循跡                │        │
│  │         (0x05)               │    │         (0x06)               │       │
│  │                              │    │                              │       │
│  └──────────────────────────────┘    └──────────────────────────────┘       │
│                                                                             │
│  ┌──────────────────────────────┐    ┌──────────────────────────────┐       │
│  │                              │    │                              │       │
│  │          開燈                │    │          關燈                │        │
│  │         (0x07)               │    │         (0x08)               │       │
│  │                              │    │                              │       │
│  └──────────────────────────────┘    └──────────────────────────────┘       │
│                                                                             │
│  ┌────────────────────────────────────┐    ┌─────────────────────────────┐  │
│  │                                    │    │             ▲               │  │
│  │  手動調試              HEX          │    │                             │  │
│  │                      (發 0x00~0x08)│    │       ◀     STOP     ▶     │  │
│  │                              ○     │    │                             │  │
│  │                                    │    │             ▼               │  │
│  └────────────────────────────────────┘    └─────────────────────────────┘  │
│                                                                             │
│  ─────────────────────────────────────────────────────────────────────────  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

```

### 1.6.4 蓝牙连接核心流程（Kotlin 示例）

```kotlin
// 1. 声明权限（AndroidManifest.xml）
// <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
// <uses-permission android:name="android.permission.BLUETOOTH_SCAN" />

// 2. 获取 BluetoothAdapter 并扫描设备
val bluetoothManager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
val bluetoothAdapter = bluetoothManager.adapter

// 3. 建立 RFCOMM 套接字连接
private suspend fun connectDevice(device: BluetoothDevice): BluetoothSocket? {
    val uuid = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB") // SPP UUID
    return withContext(Dispatchers.IO) {
        try {
            device.createRfcommSocketToServiceRecord(uuid).also { it.connect() }
        } catch (e: IOException) { null }
    }
}

// 4. 协议发送（封装帧结构）
private fun sendCommand(cmd: Byte, data: ByteArray) {
    val frame = ByteBuffer.allocate(6 + data.size).apply {
        put(0xAA.toByte()); put(0x55.toByte())      // 帧头
        put(cmd); put(data.size.toByte())           // CMD + LEN
        put(data)                                   // DATA
        var checksum = (cmd + data.size).toByte()
        data.forEach { checksum = (checksum + it).toByte() }
        put(checksum)                               // 校验
        put(0x55.toByte()); put(0xAA.toByte())      // 帧尾
    }.array()
    bluetoothSocket?.outputStream?.write(frame)
}
```

---

## 1.7 关键算法实现

### 1.7.1 循迹算法 —— 加权位置偏差法

```c
/* 8路循迹传感器：左侧→右侧 = bit0→bit7 */
/* 权值数组：越靠左权值越负，越靠右越正，中心为0 */
const int8_t weight[8] = {-7, -5, -3, -1, 1, 3, 5, 7};

/* 计算位置偏差：范围 -28 ~ +28 */
int32_t Track_CalcError(uint8_t sensor_val) {
    int32_t sum = 0, cnt = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (sensor_val & (1 << i)) {   /* 检测到黑线 */
            sum += weight[i];
            cnt++;
        }
    }
    if (cnt == 0) return 9999;          /* 丢失赛道标记 */
    return sum / cnt;                   /* 加权平均偏差 */
}
```

### 1.7.2 位置式 PID 转向控制

```c
typedef struct {
    float Kp, Ki, Kd;
    float err, last_err, integral;
    float out_max, out_min;
} PID_t;

float PID_Calc(PID_t *pid, float target, float actual) {
    pid->err = target - actual;
    pid->integral += pid->err;
    /* 积分限幅 */
    if (pid->integral >  500) pid->integral =  500;
    if (pid->integral < -500) pid->integral = -500;

    float out = pid->Kp * pid->err
              + pid->Ki * pid->integral
              + pid->Kd * (pid->err - pid->last_err);
    pid->last_err = pid->err;

    /* 输出限幅 */
    if (out > pid->out_max) out = pid->out_max;
    if (out < pid->out_min) out = pid->out_min;
    return out;
}

/* 应用：偏差→左右轮差速 */
void Motor_ControlByTrack(int32_t track_err) {
    static PID_t pid = {.Kp = 2.5f, .Ki = 0.05f, .Kd = 0.8f,
                        .out_max =  800, .out_min = -800};
    float base_speed = 600;   /* 基础速度 PWM */
    float delta = PID_Calc(&pid, 0, track_err); /* 目标偏差=0 */

    /* 弯道减速：偏差越大，基础速度越低 */
    float curve_factor = 1.0f - (fabsf(track_err) / 60.0f);
    if (curve_factor < 0.4f) curve_factor = 0.4f;
    base_speed *= curve_factor;

    int32_t left_pwm  = (int32_t)(base_speed + delta);
    int32_t right_pwm = (int32_t)(base_speed - delta);
    Motor_SetPWM(left_pwm, right_pwm);
}
```

### 1.7.3 HC-SR04 超声波测距

```c
uint16_t HCSR04_Read(void) {
    uint32_t time_ms = 0;
    /* Trig 触发：拉低 2us → 拉高 10us → 拉低 */
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
    delay_us(2);
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);

    /* 等待 Echo 高电平，超时 100ms */
    uint32_t t0 = HAL_GetTick();
    while (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_RESET) {
        if (HAL_GetTick() - t0 > 100) return 0xFFFF; /* 超时 */
    }
    /* 计时高电平宽度 */
    uint32_t t1 = HAL_GetTick();
    while (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET) {
        if (HAL_GetTick() - t1 > 100) return 0xFFFF;
    }
    time_ms = HAL_GetTick() - t1; /* 注意：建议用定时器 us 级计时 */
    /* 距离(cm) = 时间(us) * 340(m/s) / 2 / 10000 ≈ time_us / 58 */
    return (uint16_t)(time_ms * 1000 / 58);
}
```

---

## 1.8 测试与性能指标

### 1.8.1 测试项目

| 序号 | 测试项目 | 测试方法 | 预期结果 |
|------|----------|----------|----------|
| 1 | 蓝牙连接距离 | 在开阔地带逐步远离 | ≥10m 仍可稳定遥控 |
| 2 | WiFi 连接距离 | 同上 | ≥50m |
| 3 | 循迹直道 | 在 5m 直道全速行驶 | 无明显蛇形摆动，偏差≤2cm |
| 4 | 循迹 90° 弯 | R=30cm 弯道 | 顺利通过不冲出赛道 |
| 5 | 循迹 S 弯 | 5 个连续变向弯 | 顺利通过 |
| 6 | 避障制动 | 前方 15cm 放障碍 | 安全停车无碰撞 |
| 7 | 灯光功能 | APP 依次操作按键 | 对应灯光正确点亮/熄灭 |
| 8 | 低压报警 | 模拟 9.5V 电压 | APP 报警提示，保护停机 |

---

---

# 第二部分 智能救援小车

## 2.1 项目概述

本设计面向**大学生工程实践与创新大赛——智能救援赛道**，设计一款具备**高速机动、复杂地形通过、多源目标识别、安全转运**能力的全地形智能救援小车。系统以 STM32H743 + 上位机（Raspberry Pi 4B / Jeston Nano）为异构计算核心，融合激光雷达、深度相机、多光谱传感、AI 视觉识别等先进技术，可在废墟、楼梯、碎石等复杂救援场景下自主搜索被困目标，完成识别、定位、信息采集与转运任务。

### 2.1.1 设计目标

| 指标项 | 目标参数 |
|--------|----------|
| 最大平地速度 | ≥ 5 m/s |
| 最大爬坡角度 | ≥ 35° |
| 越障高度 | ≥ 20 cm（台阶） |
| 最大续航 | ≥ 90 分钟 |
| 目标识别种类 | 二维码 / 条码 / 文字 / 颜色 / 形状 / 人员轮廓 / 温度异常 / 振动源 |
| 二维码识别距离 | 0.2 ~ 5 m |
| 温度检测范围 | -40 ℃ ~ +300 ℃ |
| 定位精度 | ≤ 10 cm（融合 SLAM） |
| 有效载荷 | ≥ 5 kg（救援物资/转运平台） |

---

## 2.2 功能需求分析

### 2.2.1 功能模块图

```
┌───────────────────────────────────────────────────────────────────┐
│                       智能救援小车功能体系                          │
├───────────────────┬───────────────────┬───────────────────────────┤
│  高速运动与越障    │  多源目标识别      │  搜索定位与信息获取         │
│  ├ 高速直驱底盘   │  ├ 二维码识别      │  ├ SLAM 自主建图导航        │
│  ├ 差速/阿克曼转向│  ├ 一维码识别      │  ├ GPS/北斗室外定位         │
│  ├ 台阶/楼梯越障  │  ├ 中文/英文字识别 │  ├ UWB 室内高精度定位       │
│  ├ 斜坡/碎石通过  │  ├ 形状颜色识别    │  ├ 救援目标搜索策略         │
│  └ 原地旋转       │  ├ 红外热成像测温  │  └ 目标 GPS 坐标上传        │
├───────────────────┼───────────────────┼───────────────────────────┤
│  救援转运机构      │  安全保护机制      │  通信与远程操控             │
│  ├ 机械臂抓取     │  ├ 碰撞检测保护    │  ├ 5G / WiFi6 高清图传      │
│  ├ 升降转运平台   │  ├ 失控保护        │  ├ 433MHz 数传备份链路      │
│  ├ 物资投放装置   │  ├ 低电返航        │  ├ 图传遥控器双冗余控制      │
│  └ 声光呼救模块   │  ├ 电子围栏        │  └ 多节点自组网             │
└───────────────────┴───────────────────┴───────────────────────────┘
```

### 2.2.2 详细功能说明

#### （1）高速移动与越障
  **高速底盘**：四轮独立驱动（轮毂电机 / 行星减速电机），最大速度 ≥ 5 m/s
  **主动悬挂**：每轮独立弹簧 + 阻尼器减震，适应不平整地形
  **麦克纳姆轮（可选）**：实现全向移动，适合狭小空间精确定位
  **履带切换机构（可选）**：极端地形下切换履带模式增强通过性
  **台阶越障**：利用摆臂机构或摇臂式底盘翻上 20 cm 高台阶

#### （2）避障功能
  360° 激光雷达（LDS）构建二维障碍地图
  深度相机（RealSense D435i）获取三维点云补盲
  近距红外/超声波冗余测距
  动态障碍物速度预测与路径重规划

#### （3）救援目标搜索与转运
  **搜索策略**：螺旋覆盖搜索 → 分区栅格搜索 → 可疑区域重点复核
  **目标确认**：多传感器联合判定（视觉 + 红外 + 振动 + 生命体征（可选雷达））
  **转运机构**：
    3~6 自由度机械臂 + 夹爪（抓取小型目标）
    电动升降货叉 / 拖板（托盘式标准救援物资箱）
    目标固定绑带 + 触发释放机构

#### （4）对象识别与信息获取
| 识别类型 | 传感器方案 | 处理方式 | 输出信息 |
|----------|-----------|----------|----------|
| 二维码 QR / DM | 全局快门相机 + 补光灯 | OpenCV + ZBar / ZXing | 内容文本 + 置信度 |
| 一维条码 Code128/EAN | 工业级条码扫描头（串口） | 硬件解码 | 条码内容 |
| 中文 / 英文文字 | 高分辨率相机 + 光源 | PaddleOCR / Tesseract | 文字内容 + 位置 |
| 图像 / 人员识别 | RGB 相机 | YOLOv8 / MobileNet-SSD | 类别 + bbox + 置信度 |
| 形状（圆/方/三角） | 工业相机 | OpenCV findContours/Hough | 形状类别 + 尺寸 |
| 颜色（红/黄/绿/蓝） | 工业相机 | HSV 阈值分割 + 矩 | 颜色标签 + 面积坐标 |
| 温度（目标/环境） | MLX90640 热成像阵列（32×24）或 FLIR 单光 | 像素温度插值 | 温度矩阵 + 最高温点坐标 |
| 振动 / 生命迹象 | SW-420 振动传感器 + SYN480R 雷达（可选） | 信号频谱分析 | 振动强度 / 生命特征概率 |

#### （5）安全保护功能
| 保护类型 | 实现方案 |
|----------|----------|
| **碰撞保护** | 四周机械碰撞触边开关 + 加速度冲击检测 → 立即断电 + 反向退避 |
| **失控保护** | 通信心跳超时 3 秒 → 停车制动；姿态角度 > 45° → 切断动力 |
| **低电保护** | 电量 < 20% → 报警；<10% → 自动返航充电点；<5% → 紧急停车 |
| **电子围栏** | GPS/栅格边界限定，越界立即停车 |
| **过载保护** | 电机电流 > 阈值 → 降流 / 停转 |
| **急停按钮** | 蘑菇头急停（硬件常闭）+ APP 虚拟急停（软件双保险） |

---

## 2.3 系统总体架构

### 2.3.1 异构计算架构

```
┌──────────────────────────────────────────────────────────────────────┐
│                     上层感知与决策层（上位机）                         │
│  ┌───────────────────────┐  ┌────────────────────────────────────┐   │
│  │ Raspberry Pi 4B (8G)  │  │ 或 NVIDIA Jetson Orin Nano         │   │
│  │  OS: Ubuntu 22.04 LTS │  │  OS: JetPack 6.0 / L4T R36         │   │
│  │  ┌─────────────────┐  │  │  ┌──────────────────────────────┐  │   │
│  │  │ ROS2 Humble 节点 │  │  │  │  ROS2 + CUDA 加速推理        │  │   │
│  │  │ • SLAM 建图      │  │  │  │ • YOLOv8 目标检测(TensorRT)  │  │   │
│  │  │ • 导航 Nav2      │  │  │  │ • PaddleOCR                 │  │   │
│  │  │ • 目标识别       │  │  │  │ • 热成像温度分析              │  │   │
│  │  │ • 搜索策略       │  │  │  │ • 三维点云分割                │  │   │
│  │  └────────┬────────┘  │  │  └──────────────┬───────────────┘  │   │
│  └───────────┼───────────┘  └─────────────────┼──────────────────┘   │
│              │  千兆以太网 / USB3.0            │                       │
└──────────────┼─────────────────────────────────┼──────────────────────┘
               │                                 │
┌──────────────▼─────────────────────────────────▼──────────────────────┐
│                     实时控制层（下位机 STM32）                          │
│  ┌────────────────────────────────────────────────────────────────┐   │
│  │ STM32H743IIT6 (480MHz, Cortex-M7, 双 CAN FD)                   │   │
│  │  ┌────────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────────┐  │   │
│  │  │FreeRTOS 调度│ │FOC 电机控│ │姿态解算  │ │安全状态机         │  │  │
│  │  │(10kHz 周期) │ │  制(4路) │ │(AHRS 9轴)│ │(碰撞/失控/低电)│ │  │  |
│  │  └──────┬─────┘ └────┬─────┘ └────┬─────┘ └───────┬─────────┘ │  │
│  └─────────┼─────────────┼───────────┼───────────────┼───────────┘  │
└────────────┼─────────────┼───────────┼───────────────┼──────────────┘
             │             │           │               │
┌────────────▼─────────────▼───────────▼───────────────▼──────────────┐
│                        执行与传感层                                  │
│  ┌──────────┐ ┌──────────┐ ┌───────────┐ ┌───────────┐ ┌─────────┐ │
│  │ 轮毂电机 │ │机械臂舵机  │ │ 升降货叉  │ │ 声光报警   │ │急停回路  │ │
│  │ (FOC驱动)│ │(DRV8323)  │ │(丝杆步进) │ │(LED+蜂鸣) │ │(蘑菇头)  │ │
│  └──────────┘ └──────────┘ └───────────┘ └───────────┘ └─────────┘ │
│  ┌──────────┐ ┌──────────┐ ┌───────────┐ ┌───────────┐ ┌─────────┐ │
│  │ 激光雷达  │ │深度相机   │ │ 工业相机  │ │ 热成像阵列 │ │GPS+IMU  │ │
│  │ (LDS)    │ │(D435i)   │ │(全局快门) │ │(MLX90640)  │ │(NEO-M9N)│ │
│  └──────────┘ └──────────┘ └───────────┘ └───────────┘ └─────────┘ │
│  ┌──────────┐ ┌──────────┐ ┌───────────┐ ┌───────────┐             │
│  │温度传感   │ │振动传感  │ │ 碰撞触边   │ │ 电流/电压  │             │
│  │(NTC×8路) │ │(SW-420)  │ │ (×8 路)   │ │ (INA226)  │             │
│  └──────────┘ └──────────┘ └───────────┘ └───────────┘             │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 2.4 硬件设计方案

### 2.4.1 主控与计算单元

| 模块 | 型号 | 关键参数 | 作用 |
|------|------|----------|------|
| 运动控制 MCU | STM32H743IIT6 | 480MHz, 2MB Flash, 1MB SRAM, 双 CAN FD | 电机控制、安全保护、传感器采集、通信桥接 |
| AI 上位机 | Raspberry Pi 4B 8GB / Jetson Orin Nano 8GB | CPU 4×A76 / GPU 1024 CUDA | 视觉识别、SLAM 建图、导航决策、图传 |
| 供电主控 | Arduino Nano Every（可选冗余） | ATmega4809 | 独立的电源监控与硬急停逻辑 |

### 2.4.2 动力底盘系统

#### 方案 A：四轮独立驱动 + 主动悬挂（推荐）
| 部件 | 型号 | 参数 |
|------|------|------|
| 轮毂电机 | 大疆 M3508 或同级别 24V 无刷 | 额定扭矩 0.7 N·m，峰值 2.5 N·m，带编码器 |
| FOC 驱动器 | C610 / 自制 DRV8323 方案 | CAN 总线控制，FOC 闭环 |
| 悬挂系统 | 独立双叉臂 + 60mm 行程阻尼 | 每轮独立 |
| 减速比 | 1:27 行星减速 | 匹配车轮转速 |
| 车轮 | 150mm 防滑橡胶轮（可选履带套件） | 深花纹抓地 |

#### 方案 B：麦克纳姆轮全向底盘
  4 轮 Mecanum 轮（辊子斜 45°）+ 4 路独立 FOC
  支持 X/Y/θ 三自由度零半径移动

#### 摆臂越障机构
  左右独立电动摆臂（舵机或行星减速电机驱动）
  摆臂旋转角度 ±180°，翻越台阶时前轮搭台后驱动主体跟进

### 2.4.3 救援执行机构

```
机械臂（URDF 示例构型）：
┌─ J0 底盘旋转关节 (0~360°, 舵机 RDS5160 60kg)
└─ J1 大臂关节 (-30°~+90°, 舵机 30kg)
   └─ J2 小臂关节 (-90°~+90°, 舵机 20kg)
      └─ J3 腕部旋转 (0~360°)
         └─ J4 夹爪开合 (行程 120mm, 夹持力 ≥ 50N)

升降转运平台：
├ 直流丝杆滑台 + 电机（行程 300mm，载重量 ≥ 20kg）
└ 光电限位开关 (上/下极限) + 霍尔位置反馈
```

### 2.4.4 感知传感阵列

#### （1）视觉组
| 传感器 | 型号 | 接口 | 分辨率 | 视场 | 安装位 |
|--------|------|------|--------|------|--------|
| 前视识别相机 | 海康威视 MV-CA060-10GC / 工业 USB3 | USB 3.0 GigE | 600 万像素全局快门 | 60° | 前顶云台（两自由度舵机±90°） |
| 深度相机 | Intel RealSense D435i | USB 3.0 | 1280×720 深度 | 87°×58° | 前部中轴 |
| 热成像阵列 | Melexis MLX90640ESF-BAB | I2C (400kHz) | 32×24 像素 | 110°×75° | 云台相机旁 |
| 条码扫描头 | 新大陆 NLS-EM1395 | TTL UART | 1280×800 | 70° | 前端 |

#### （2）激光与定位组
| 传感器 | 型号 | 参数 | 接口 |
|--------|------|------|------|
| 激光雷达 | 乐动 LD19 / 思岚 RPLIDAR A1/A3 | 测距 12m，10Hz，360° | UART / USB |
| 高级 SLAM 雷达（可选） | 速腾聚创 RP-Lidar S3 / Livox Mid-360 | 测距 40m，点云密度高 | Ethernet |
| GPS/北斗双模 | U-blox NEO-M9N + 螺旋天线 | 水平精度 1.5m CEP | UART (9600, NMEA) |
| UWB 室内定位 | DWM1000 模块（基站×3 + 标签×1） | 精度 10 cm | SPI |
| 9 轴 IMU | ICM-42688-P + AK09916 (或 BNO085) | 加速度 16g，陀螺仪 ±2000dps，磁力计 | SPI |

#### （3）环境与安全传感
| 传感器 | 型号 | 数量 | 用途 |
|--------|------|------|------|
| 超声波测距 | HC-SR04 / JSN-SR04T | 8（四向各 2） | 近距补盲 |
| 碰撞触边开关 | 常闭型机械开关条 | 4（四周各一条） | 物理碰撞触发 |
| 振动传感器 | SW-420 或 ADXL355 | 4（底盘四角） | 振动源定位 |
| 非接触温度（单点） | MLX90614ESF-BAA | 4 | 环境/部件温度监测 |
| 烟雾检测 | MQ-2 | 1 | 危险环境预警 |
| 电流电压检测 | INA226 | 6（总母线 + 5 分路） | 过流/过压/电量监测 |

### 2.4.5 通信链路（三冗余）

| 链路 | 方案 | 带宽/速率 | 用途 |
|------|------|-----------|------|
| 主链路（图传+控制） | 5G 模组（MH5000）/ WiFi 6 AX200 | 下行 ≥100Mbps | 视频推流、远程桌面、遥测 |
| 备份链路（纯控制） | 433MHz LoRa（SX1278，5W 功放） | 1.2~19.2 kbps | 紧急停车、基础运动指令 |
| 近距调试 | USB / UART 数传 915MHz | 921600 bps | 调试、参数配置 |
| 自组网（多车） | ESP-NOW / Zigbee 3.0 | — | 多车协同调度 |

### 2.4.6 电源系统

```
┌─────────────────────────────────────────────────────────────┐
│  动力电池组（主电）                                           │
│  6S1P / 6S2P 21700 Li-ion (22.2V, 10.4Ah)                   │
│  BMS: 6S 100A 带均衡 + 温度保护                               │
│    │                                                         │
│    ├── DC-DC 24V 30A（隔离）→ 轮毂电机驱动器总线               │
│    │                                                         │
│    ├── DC-DC 12V 15A → 机械臂舵机、升降平台、灯光、蜂鸣器       │
│    │                                                         │
│    ├── DC-DC 5V 10A  → 激光雷达、相机、USB Hub                │
│    │                                                         │
│    └── DC-DC 5V 6A (UPS) → Raspberry Pi / Jetson + SSD       │
│           │                                                  │
│           └── 18650 2S 小电池(2000mAh) 作为 Pi UPS 缓冲       │
└─────────────────────────────────────────────────────────────┘
```

---

## 2.5 软件设计方案

### 2.5.1 软件分层架构

```
┌──────────────────────────────────────────────────────────────┐
│                    用户操作层（地面站 / 遥控器）               │
│   QGroundControl / 自研 WebGis / 图传遥控器 Android APP      │
└──────────────────────────────┬───────────────────────────────┘
                               │ MAVLink 2.0 / WebRTC
┌──────────────────────────────▼───────────────────────────────┐
│                    上位机（Raspberry Pi / Jetson）            │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │                   ROS 2 Humble 计算图                   │ │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────┐ │ │
│  │  │ slam_tool│ │ nav2 导  │ │ vision_  │ │ mission_   │ │ │
│  │  │ box (LOAM│ │ 航栈     │ │ identify │ │ planner    │ │ │
│  │  │ /SLAM_3D)│ │(全局+局  │ │ 包       │ │ (搜索策略) │ │ │
│  │  │          │ │部规划器) │ │          │ │            │ │ │
│  │  └────┬─────┘ └────┬─────┘ └────┬─────┘ └─────┬──────┘ │ │
│  │       │            │            │              │        │ │
│  │  ┌────▼────────────▼────────────▼──────────────▼──────┐ │ │
│  │  │              话题 / 服务通信中间件 DDS             │ │ │
│  │  └────┬───────────────────────────────────────────────┘ │ │
│  │       │                                               │ │
│  │  ┌────▼───────────────────────────────────────────────┐ │ │
│  │  │              micro-ROS Agent 桥接                  │ │ │
│  │  └────┬───────────────────────────────────────────────┘ │ │
│  └───────┼─────────────────────────────────────────────────┘ │
└──────────┼───────────────────────────────────────────────────┘
           │ 串口 / CAN (micro-ROS XRCE-DDS / 自定义协议)
┌──────────▼───────────────────────────────────────────────────┐
│                下位机（STM32H743 + FreeRTOS）                │
│  ┌─────────┐ ┌─────────┐ ┌──────────┐ ┌───────────────────┐ │
│  │ 电机控  │ │ 姿态解  │ │ 安全状态  │ │ 采集/通信(微ROS)  │ │
│  │ 制任务  │ │ 算任务  │ │ 机任务    │ │ 任务              │ │
│  │ (20kHz) │ │(1kHz)   │ │(500Hz)   │ │ (各传感器驱动)    │ │
│  └─────────┘ └─────────┘ └──────────┘ └───────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

### 2.5.2 下位机 FreeRTOS 任务划分（STM32）

```c
/* 任务优先级（数字越大优先级越高） */
#define TASK_PRIO_SAFETY     (configMAX_PRIORITIES - 1)  /* 安全监控最高 */
#define TASK_PRIO_MOTOR_FOC  (configMAX_PRIORITIES - 2)  /* FOC 电流环 */
#define TASK_PRIO_IMU        (configMAX_PRIORITIES - 3)  /* IMU 采样 */
#define TASK_PRIO_MOTION     (configMAX_PRIORITIES - 4)  /* 速度/位置环 */
#define TASK_PRIO_COMM       (configMAX_PRIORITIES - 5)  /* 通信协议解析 */
#define TASK_PRIO_SENSOR     (tskIDLE_PRIORITY + 3)      /* 低采样率传感器 */
#define TASK_PRIO_LED        (tskIDLE_PRIORITY + 1)      /* 指示灯 */
```

| 任务名 | 周期 | 核心功能 |
|--------|------|----------|
| `Task_Safety` | 2 ms | 急停、碰撞、姿态、电流、通信心跳综合判定，输出使能信号 |
| `Task_MotorFOC` | 50 μs | FOC 电流环（Clark/Park 变换 → SVPWM） |
| `Task_IMU` | 1 ms | BMI088/ICM42688 读取 → 姿态四元数 Mahony/Madgwick 滤波 |
| `Task_Motion` | 10 ms | 串级 PID（位置→速度→电流）+ 底盘解算 |
| `Task_Comm` | 5 ms | micro-ROS 发布订阅 + 自定义遥测 |
| `Task_Sensor` | 50 ms | 温度、振动、电量、超声波、GPS 采集 |

### 2.5.3 下位机与上位机通信协议

采用 **micro-ROS (XRCE-DDS over UART/CAN)** 为主，自定义 **MAVLink 兼容协议**为备份：

**关键 ROS 2 Topic：**

| Topic 名 | 消息类型 | 发布者 | 频率 | 说明 |
|----------|----------|--------|------|------|
| `/cmd_vel` | geometry_msgs/Twist | 上位机 Nav2 | 50Hz | 目标速度指令 |
| `/odom` | nav_msgs/Odometry | 下位机 | 100Hz | 里程计（轮速+IMU融合） |
| `/imu/data` | sensor_msgs/Imu | 下位机 | 200Hz | IMU 原始+姿态 |
| `/scan` | sensor_msgs/LaserScan | 上位机雷达节点 | 10Hz | 激光雷达数据 |
| `/safety/status` | custom_msgs/SafetyStatus | 下位机 | 20Hz | 安全状态字（见下） |
| `/target/info` | custom_msgs/TargetInfo | 上位机识别节点 | 10Hz | 识别到的目标信息 |

**安全状态字 SafetyStatus.status_flag（位域）：**

| Bit | 标志 | 置位含义 | 处理 |
|-----|------|----------|------|
| 0 | ESTOP_HW | 硬件蘑菇头拍下 | 立即切断主接触器 |
| 1 | ESTOP_SW | 软件急停指令 | 立即切断 PWM 输出 |
| 2 | COLLISION | 碰撞触边触发 | 反向退避 300ms，停车 |
| 3 | TILT | 侧翻风险（roll/pitch > 40°） | 切断动力，制动 |
| 4 | LOST_COMM | 上位机心跳超时 > 3s | 维持当前状态 → 超时停车 |
| 5 | OVER_CURRENT | 母线过流 > 60A | 降额限流 |
| 6 | LOW_BATTERY | 电量 < 10% | 立即返航 |
| 7 | GEOFENCE | 超出电子围栏 | 立即停车 |

---

## 2.6 多传感器融合与目标识别

### 2.6.1 目标识别流水线（上位机）

```
┌─────────────┐    ┌─────────────────┐    ┌──────────────────────┐
│ 图像采集    │───→│ 图像预处理      │───→│ 多任务并行推理引擎   │
│ (多相机)    │    │ • 畸变校正      │    │ ┌──────────────────┐ │
│             │    │ • 白平衡/去噪   │    │ │ YOLOv8 目标检测 │ │
│ 25~60 fps   │    │ • ROI 裁剪      │    │ │ (人/形状/颜色)  │ │
└─────────────┘    └─────────────────┘    │ └────────┬─────────┘ │
                                          │ ┌────────▼─────────┐ │
                                          │ │ PaddleOCR 文字识 │ │
                                          │ │ 别(中/英/数字)   │ │
                                          │ └────────┬─────────┘ │
                                          │ ┌────────▼─────────┐ │
                                          │ │ ZXing 二维码/条  │ │
                                          │ │ 码解码           │ │
                                          │ └────────┬─────────┘ │
                                          │ ┌────────▼─────────┐ │
                                          │ │ 热成像温度异常   │ │
                                          │ │ 检测             │ │
                                          │ └────────┬─────────┘ │
                                          └──────────┼───────────┘
                                                     ▼
┌──────────────────────┐    ┌─────────────────────────────────────┐
│ 目标信息入库         │←───│ 多传感器证据融合（D-S 证据理论）     │
│ (SQLite + 时间戳)    │    │ • 视觉置信度 × 红外温度 × 振动强度  │
└──────────┬───────────┘    │ → 综合目标存在概率 P(目标|证据)     │
           ▼                │ → 目标定位（像素→相机坐标系→世界） │
┌──────────────────────┐    └─────────────────────────────────────┘
│ 上报地面站 / 云端     │
│ (WebRTC + HTTP POST) │
└──────────────────────┘
```

### 2.6.2 YOLOv8 目标检测示例（Python）

```python
# ultralytics YOLOv8 部署示例
from ultralytics import YOLO
import cv2, numpy as np

# 模型加载（可切换 TensorRT / ONNX / NCNN 加速）
model = YOLO("yolov8n-rescue.pt")   # 针对救援场景微调的权重
model.to("cuda")                     # Jetson 用 CUDA，树莓派切 CPU

# 目标类别：救援场景自定义
RESCUE_CLASSES = {
    0: "person",          # 被困人员
    1: "survivor_marker", # 救援标志物（二维码牌）
    2: "hazard_box",      # 危险物品箱
    3: "red_target",      # 红色目标（颜色）
    4: "circle_target",   # 圆形目标（形状）
}

def detect_rescue_targets(frame_rgb):
    """返回目标列表：[ {"cls":..., "conf":..., "xyxy":..., "center_xy":...} ]"""
    results = model(frame_rgb, conf=0.4, iou=0.5, verbose=False)[0]
    targets = []
    for box in results.boxes:
        cls_id = int(box.cls[0].item())
        targets.append({
            "cls_id": cls_id,
            "class": RESCUE_CLASSES.get(cls_id, f"unk_{cls_id}"),
            "confidence": float(box.conf[0].item()),
            "xyxy": box.xyxy[0].cpu().numpy().astype(int).tolist(),
            "center_px": box.xywh[0][:2].cpu().numpy().astype(int).tolist(),
        })
    return targets
```

### 2.6.3 二维码 / 条码识别

```python
# pyzbar / zxing-cpp 多码制识别
from pyzbar.pyzbar import decode, ZBarSymbol

def decode_any_code(gray_img):
    """识别 QR / DataMatrix / Code128 / EAN 等"""
    results = decode(gray_img, symbols=[
        ZBarSymbol.QRCODE,
        ZBarSymbol.DATAMATRIX,
        ZBarSymbol.CODE128,
        ZBarSymbol.EAN13,
    ])
    return [{
        "data": r.data.decode("utf-8", errors="ignore"),
        "type": r.type,
        "polygon": [(p.x, p.y) for p in r.polygon],
        "rect": r.rect,
    } for r in results]
```

### 2.6.4 颜色 / 形状识别（OpenCV）

```python
import cv2
import numpy as np

# 目标颜色 HSV 范围（需在灯光下标定）
COLOR_RANGES = {
    "red":    [(0, 100, 80),   (10, 255, 255)],
    "yellow": [(20, 100, 100), (35, 255, 255)],
    "green":  [(40, 50, 80),   (85, 255, 255)],
    "blue":   [(100, 100, 80), (130, 255, 255)],
}

def detect_color_shape(bgr_img):
    hsv = cv2.cvtColor(bgr_img, cv2.COLOR_BGR2HSV)
    out_list = []
    for color_name, (lo, hi) in COLOR_RANGES.items():
        mask = cv2.inRange(hsv, np.array(lo), np.array(hi))
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, np.ones((5,5), np.uint8))
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        for c in contours:
            area = cv2.contourArea(c)
            if area < 300: continue
            peri = cv2.arcLength(c, True)
            approx = cv2.approxPolyDP(c, 0.04*peri, True)
            n = len(approx)
            if   n == 3: shape = "triangle"
            elif n == 4: shape = "square"
            else:
                circ = 4*np.pi*area/(peri*peri)
                shape = "circle" if circ > 0.85 else f"polygon_{n}"
            x,y,w,h = cv2.boundingRect(c)
            out_list.append({"color": color_name, "shape": shape,
                             "area": area, "bbox": [x,y,w,h]})
    return out_list
```

### 2.6.5 红外热成像温度读取（MLX90640）

```python
# sudo pip3 install mlx90640-driver smbus2 numpy
import numpy as np
from smbus2 import SMBus
import mlx90640

def thermal_init(i2c_bus=1, addr=0x33):
    dev = mlx90640.MLX90640()
    dev.i2c_init(f"/dev/i2c-{i2c_bus}", 800000)
    dev.setRefreshRate(16)   # 16 Hz
    return dev

def thermal_read(dev):
    frame = [0]*768  # 32x24
    dev.getFrameData(frame)
    emissivity = 0.95
    tr = 23.15       # 环境反射温度(K)
    temps = dev.calculateTo(frame, emissivity, tr) - 273.15   # → ℃
    img = np.array(temps).reshape(24, 32).astype(np.float32)
    return {
        "matrix": img,
        "t_min": float(img.min()),
        "t_max": float(img.max()),
        "t_mean": float(img.mean()),
        "hot_spot_xy": np.unravel_index(img.argmax(), img.shape),
    }
```

### 2.6.6 振动信号分析（被困人员敲击信号识别）

```python
import numpy as np
from scipy import signal

def analyze_vibration(samples: np.ndarray, fs=2000):
    """
    samples: 振动 ADC 采样序列 (N 点)
    fs: 采样率 Hz
    返回：振动强度、是否存在周期性敲击
    """
    # 1. 时域能量
    rms = np.sqrt(np.mean(samples**2))
    peak = np.max(np.abs(samples))

    # 2. 频域分析（FFT）
    freqs, psd = signal.welch(samples, fs=fs, nperseg=1024)
    band_5_30 = psd[(freqs>5) & (freqs<30)].sum()   # 人工敲击频带
    total_energy = psd.sum() + 1e-9

    # 3. 周期性自相关检测（敲击间隔 1~2s）
    corr = np.correlate(samples - samples.mean(),
                        samples - samples.mean(), mode="full")
    corr = corr[len(corr)//2:]
    peaks, _ = signal.find_peaks(corr, height=0.3*corr.max(),
                                 distance=int(fs*0.8))   # ≥0.8s 间隔
    periodic = len(peaks) >= 3

    return {
        "rms_strength": float(rms),
        "peak_strength": float(peak),
        "knock_band_ratio": float(band_5_30 / total_energy),
        "periodic_knock_detected": periodic,
        "knock_count": len(peaks),
    }
```

---

## 2.7 运动控制与越障策略

### 2.7.1 四轮差速底盘解算

```c
/* 底盘解算：目标 Twist(vx, vy, w) → 4 轮目标转速(rpm) */
/* 麦轮全向底盘 (辊子45°)： */
#define WHEEL_R        0.075f   /* 轮半径 m    */
#define WHEEL_BASE_L   0.300f   /* 前后轴距 m  */
#define WHEEL_BASE_W   0.260f   /* 左右轮距 m  */

void Mecanum_InverseKinematics(float vx, float vy, float omega,
                               float wheel_rpm[4]) {
    const float k = 1.0f / WHEEL_R;                 /* 转速常数 */
    const float L_W = (WHEEL_BASE_L + WHEEL_BASE_W) / 2.0f;
    /* 轮序：0=FL 前左, 1=FR 前右, 2=RL 后左, 3=RR 后右 */
    wheel_rpm[0] = k * ( vx - vy - L_W * omega) * 60.0f / (2.0f*3.14159f);
    wheel_rpm[1] = k * ( vx + vy + L_W * omega) * 60.0f / (2.0f*3.14159f);
    wheel_rpm[2] = k * ( vx + vy - L_W * omega) * 60.0f / (2.0f*3.14159f);
    wheel_rpm[3] = k * ( vx - vy + L_W * omega) * 60.0f / (2.0f*3.14159f);
}
```

### 2.7.2 台阶越障状态机

```python
class StepClimbFSM:
    """台阶越障策略（带摆臂）"""
    S_IDLE       = 0  # 等待指令
    S_APPROACH   = 1  # 低速靠近台阶（测距 15cm 处停车）
    S_ARM_FRONT  = 2  # 摆臂下摆 → 搭上台面
    S_LIFT_FRONT = 3  # 主驱动前进 + 摆臂上拉 → 前轮上台
    S_DRIVE_BODY = 4  # 持续前进驱动中轮到台边
    S_ARM_REAR   = 5  # 后摆臂（若有）或利用剩余速度冲上台
    S_FINISH     = 6  # 越障完成，姿态回正

    def step(self, dist_front: float, pitch: float, imu_shock: float):
        if self.state == self.S_APPROACH:
            if dist_front < 0.12:
                self.cmd_vx = 0.0; self.state = self.S_ARM_FRONT
            else:
                self.cmd_vx = 0.2
        elif self.state == self.S_ARM_FRONT:
            self.set_swing_arm(-60)  # 摆臂下摆角度
            if self.arm_reached():
                self.state = self.S_LIFT_FRONT
        elif self.state == self.S_LIFT_FRONT:
            self.cmd_vx = 0.35
            self.set_swing_arm(20)   # 缓慢抬升
            if pitch > 15 and imu_shock < 0.5:
                self.state = self.S_DRIVE_BODY
        # ... 其余状态
        return Twist(linear_x=self.cmd_vx, angular_z=0)
```

---

## 2.8 安全保护机制

### 2.8.1 安全状态机总览

```
               ┌──────────────────┐
               │  NORMAL 正常运行  │◀──────────┐
               └───────┬──────────┘           │
                       │ 触发任一报警          │ 报警解除+复位
                       ▼                       │
               ┌──────────────────┐           │
               │ FAULT 报警状态    │───────────┘
               │ （分级处理）      │
               └───────┬──────────┘
                       │
          ┌────────────┼─────────────┐
          ▼            ▼             ▼
   一级：告警      二级：减速     三级：立即断电
  (灯闪+蜂鸣)   (限速 30%)   (主接触器断开+电机制动)
     │              │               │
     ▼              ▼               ▼
  低电<20%    低电<15%         急停/碰撞/侧翻/围栏
  过热<70℃    电流<80%阈值     通信丢失>10s
  围栏预警    连续碰撞2次       姿态>45°
```

### 2.8.2 碰撞与失控保护 C 代码（嵌入式）

```c
/* 全局安全结构 */
typedef struct {
    uint16_t flag;             /* 状态字位域，见 2.5.3 */
    uint32_t last_comm_ms;     /* 上次通信时间戳 */
    uint16_t collision_cnt;    /* 碰撞次数 */
    float    roll, pitch;      /* 姿态角 */
    float    ims_motor[4];     /* 电机电流 */
    float    bus_v;            /* 母线电压 */
} Safety_t;
Safety_t g_safety;

#define COMM_TIMEOUT_MS  3000      /* 通信心跳超时 */
#define TILT_LIMIT_DEG   40.0f     /* 侧翻角度阈值 */
#define OVER_CURRENT_A   30.0f     /* 单电机过流阈值 */

/* 安全监控回调（高优先级 500Hz）*/
void Safety_MonitorCallback(void) {
    uint32_t now = HAL_GetTick();
    Safety_t *s = &g_safety;

    /* 1. 失控：通信超时 */
    if ((now - s->last_comm_ms) > COMM_TIMEOUT_MS) {
        s->flag |= SAFETY_F_LOST_COMM;
    }

    /* 2. 失控：姿态倾角过大 */
    if (fabsf(s->roll)  > TILT_LIMIT_DEG ||
        fabsf(s->pitch) > TILT_LIMIT_DEG) {
        s->flag |= SAFETY_F_TILT;
    }

    /* 3. 碰撞：触边开关任意按下 */
    if (Bumper_Get() != 0) {
        s->flag |= SAFETY_F_COLLISION;
        s->collision_cnt++;
    }

    /* 4. 过流保护 */
    for (int i = 0; i < 4; i++) {
        if (s->ims_motor[i] > OVER_CURRENT_A) {
            s->flag |= SAFETY_F_OVER_CURRENT;
        }
    }

    /* 5. 综合决策：是否允许 PWM 输出 */
    uint16_t fatal_mask = SAFETY_F_ESTOP_HW | SAFETY_F_ESTOP_SW
                        | SAFETY_F_COLLISION | SAFETY_F_TILT;
    if (s->flag & fatal_mask) {
        Motor_StopAll();           /* 立即切断 */
        Relay_MainContactor(OFF);  /* 断开主接触器（碰撞/急停时）*/
        Buzzer_StartAlarm();
        Led_SetErrorPattern();
    } else if (s->flag & (SAFETY_F_LOST_COMM | SAFETY_F_OVER_CURRENT)) {
        Motor_SetSpeedLimit(0.3f); /* 降级 30% */
    } else {
        Motor_Enable();            /* 正常使能 */
    }
}
```

### 2.8.3 失控保护下的"安全停车"策略

```c
/* 通信丢失后按时间梯度采取保护动作 */
void Safety_LostCommHandle(uint32_t lost_ms) {
    if (lost_ms < 1000) {
        /* 1s 内维持最后指令，允许短时抖动 */
        Motor_HoldLastCmd();
    } else if (lost_ms < 3000) {
        /* 1~3s：线性减速至 50% */
        Motor_SetSpeedLimit(0.5f - (lost_ms-1000)/4000.0f);
    } else if (lost_ms < 6000) {
        /* 3~6s：继续缓慢减速 */
        Motor_LinearDecel(0.5f, 0.0f, 3000);
    } else {
        /* >6s：完全停车并锁定（电磁刹车）*/
        Motor_EmergencyBrake();
    }
}
```

---

## 2.9 测试与性能指标

### 2.9.1 综合测试项目表

| 大类 | 子项 | 测试条件 | 合格标准 |
|------|------|----------|----------|
| **移动性能** | 最高车速 | 平地，满载 5kg | ≥ 5 m/s |
| | 爬坡 | 35° 坡度带 | 成功登顶无溜车 |
| | 台阶越障 | 高度 20 cm | 成功翻上，用时 ≤ 10 s |
| | 碎石路面 | 粒径 3~5 cm，长度 5 m | 通过无打滑、无卡阻 |
| **避障能力** | 静态避障 | 障碍 10~50cm | 距离 ≥ 5cm 绕行无碰撞 |
| | 动态避障 | 人迎面走来 1 m/s | 提前减速/停车 |
| | 近距盲区别 | 贴墙 1cm 行驶 | 超声波+触边双保险 |
| **识别性能** | 二维码识别 | 3m 距离，光强 200lux | 成功率 ≥ 98%，耗时 ≤ 500ms |
| | 条码识别 | 50cm 扫描头正对 | 成功率 100% |
| | 文字识别（OCR） | 宋体/黑体 16pt 打印纸 | 识别率 ≥ 90% |
| | 形状颜色识别 | 4 种颜色 × 3 种形状 | 准确率 ≥ 95% |
| | 热成像测温 | 人体目标 37℃，1m 距离 | 误差 ±2℃ |
| | 振动检测 | 人工 2 次/秒敲击 | 触发检测并告警 |
| **安全保护** | 蘑菇头急停 | 高速行驶中拍下 | ≤ 100 ms 内切断动力 |
| | 碰撞触边 | 障碍从侧向撞击 | 立即停车 → 退避 → 告警 |
| | 失控心跳丢失 | 人为断开通信 | 6s 内完成安全停车 |
| | 侧翻保护 | 人工倾斜 45° | 立即断电，告警 |
| | 低电返航 | 人为设置电量 10% | 自动规划路径回充 |

---

# 两方案对比总结

| 对比维度 | 电赛循迹遥控小车（第一部分） | 智能救援小车（第二部分） |
|----------|------------------------------|--------------------------|
| **对应赛事** | 全国大学生电子设计大赛（NUEDC）循迹小车赛道 | 大学生工程实践与创新大赛 智能救援赛道 |
| **核心目标** | 循迹精度 + 稳定遥控 + 基础避障 | 全地形通过 + 多源识别 + 救援作业 + 安全冗余 |
| **主控架构** | 单核 STM32F103 单片机 | 异构双核：STM32H743 实时控制 + Pi/Jetson 上位机 AI |
| **算力水平** | 低（无 OS，裸机 ~72MHz） | 高（Linux + ROS2 + CUDA/TensorRT 推理） |
| **底盘形式** | 2~4 轮直流减速 + L298N | 四轮 FOC 独立驱动 + 主动悬挂 + 摆臂 / 麦轮 |
| **最高速度** | 约 0.5~2 m/s | ≥ 5 m/s |
| **越障能力** | 仅平整路面（小坎 ≤ 2cm） | 台阶 20cm，坡度 35°，碎石地形 |
| **感知传感器** | 8 路红外循迹 + 2 路超声波 | 激光雷达 + 深度相机 + 多工业相机 + 热成像 + GPS/UWB/IMU 等 |
| **目标识别** | 无（或仅简单颜色） | 二维码 / 条码 / OCR / YOLO 目标检测 / 形状颜色 / 温度 / 振动 |
| **通信方式** | 蓝牙 HC-05 或 ESP8266 WiFi | 三冗余：5G/WiFi6 主链路 + LoRa 备份 + 915MHz 数传 |
| **操控方式** | Android 摇杆 APP 遥控 + 自主循迹 | 地面站 QGC + 图传遥控器 + 自主 SLAM 导航（搜索策略）|
| **安全机制** | 过流、电压、循迹丢失停车 | 硬件急停 + 碰撞触边 + 姿态检测 + 通信心跳失控保护 + 电子围栏 + 分级故障状态机 |
| **续航** | ≥ 2h | ≥ 90 min |
| **开发周期** | 约 2~4 周（2 人团队） | 约 12~20 周（5~8 人团队） |
| **总成本量级** | 约 ¥300 ~ 1000 | 约 ¥2 万 ~ 10 万（根据传感器配置浮动） |
| **技术亮点** | PID 转向算法 + Android 蓝牙协议栈 + 实时循迹 | 异构计算 + ROS2 微控制器桥接 + 多传感器证据融合 + 安全状态机 |

---

> **文档结束**
> 本文档共涵盖两类智能小车设计方案：
> 1. 面向电赛的循迹遥控小车（轻量级、快速实现）
> 2. 面向智能救援赛道的全地形作业小车（综合化、AI 增强）
> 方案可根据实际赛题要求、预算和团队规模灵活裁剪相应模块。
