# STM32 智能車藍牙遙控 APP（Android）

本項目是 **Android 原生 APP（Kotlin + 經典藍牙 SPP）**，配套 STM32 智能車固件（JDY-31 / HC-05 / HC-06 等經典藍牙透傳模組，波特率 9600）實現手機遠程遙控。

> 📌 對照參考：`D:\STM32F1智能小车资料（PCB版）\4.蓝牙遥控器APP\蓝牙遥控APP.apk`

---

## 📱 APP 功能

### 主頁（遙控）
- **5 向方向鍵**（前/後/左/右 + STOP）—— **按住運動、鬆開自動停止**（按住式控制，更符合遙控車手感）
- **4 個模式快捷按鈕**：避障模式（0x05）、循跡模式（0x06）、開燈（0x07）、關燈（0x08）
- **手動調試輸入**：HEX / ASCII 雙模式切換，可手動發送任意字節序列
- **緊急停車按鈕**：直接發 0x00，不論當前模式
- **連接狀態卡片**：設備名、MAC 地址、連接狀態、已發送次數、上次發送字節、實時接收顯示

### 設備選擇頁
- 顯示已配對設備 + 掃描發現新設備
- 自動請求所需藍牙權限（Android 12+ 用 BLUETOOTH_SCAN/CONNECT，舊版用位置權限）
- 點擊任意設備直接連接

---

## 📡 指令協議（與固件完全對應）

| HEX  | ASCII | 功能 | 固件執行 |
|------|-------|------|---------|
| 0x00 | '0'   | 停止 | `Motor_Stop()` |
| 0x01 | '1'   | 前進 | `Motor_Control(50, 50)` |
| 0x02 | '2'   | 後退 | `Motor_Control(-40, -40)` |
| 0x03 | '3'   | 左轉 | `Motor_Control(-40, 40)` |
| 0x04 | '4'   | 右轉 | `Motor_Control(40, -40)` |
| 0x05 | '5'   | 避障模式 | 舵機掃描 3 方向測距 |
| 0x06 | '6'   | 循跡模式 | 紅外循跡 |
| 0x07 | '7'   | 開燈 | `LED_ON()` |
| 0x08 | '8'   | 關燈 | `LED_OFF()` |

APP 預設為 **HEX 模式**，發送 `0x01~0x08` 單字節；可在主頁切換為 **ASCII 模式**發送字符 `'0'~'8'`（0x30~0x38），固件自動映射。

---

## 🚀 環境準備（從零到編譯出 APK）

### 1. 安裝 JDK 17
- 下載地址：[Microsoft OpenJDK 17](https://learn.microsoft.com/zh-cn/java/openjdk/download)（Windows x64 .msi）
- 安裝後確認：打開 CMD 執行 `java -version`，應顯示 17.x.x

### 2. 安裝 Android Studio
- 下載地址：[Android Studio 官網](https://developer.android.com/studio)（約 1GB）
- 安裝時全部默認即可，首次啟動會自動下載 Android SDK（約 3GB，需要較長時間）

### 3. 打開項目
1. 打開 Android Studio → `File → Open`
2. 選擇目錄 `D:\stm32 project\Car_stm32\android_app`
3. 等待 Gradle 同步完成（首次會下載依賴，約 5-10 分鐘）

### 4. 連接手機（推薦真機調試）
- 手機進入「設置 → 關於手機」連點 7 次「版本號」開啟開發者模式
- 「設置 → 開發者選項」打開 **USB 調試**
- 用 USB 數據線連接電腦（手機彈窗選「允許調試」）

### 5. 編譯並安裝 APP
- 頂部工具欄選中你的手機設備
- 點擊 ▶️ Run 按鈕（或按 `Shift + F10`），Android Studio 會自動編譯並安裝到手機

### 6. 導出 APK 文件（不連接電腦也能安裝）
- 菜單：`Build → Build Bundle(s) / APK(s) → Build APK(s)`
- 編譯完成後點通知欄的「locate」連結
- APK 文件路徑：`android_app\app\build\outputs\apk\debug\app-debug.apk`
- 把這個 APK 拷到手機上，點擊安裝即可（手機需開啟「允許未知來源應用」）

---

## 📁 項目結構

```
android_app/
├── settings.gradle              # Gradle 設置
├── build.gradle                 # 項目級構建腳本
├── gradle.properties            # Gradle 屬性
└── app/
    ├── build.gradle             # 模組級構建腳本（依賴、SDK 版本）
    ├── proguard-rules.pro
    └── src/main/
        ├── AndroidManifest.xml  # 清單（權限、Activity 聲明）
        ├── java/com/car/remote/
        │   ├── MainActivity.kt        # 遙控主頁
        │   ├── DeviceListActivity.kt  # 藍牙設備選擇頁
        │   ├── BluetoothManager.kt    # 藍牙 SPP 單例管理器
        │   ├── CarCommand.kt          # 指令定義 + HEX/ASCII 轉換
        │   └── HexUtil.kt             # 十六進制工具
        └── res/
            ├── layout/
            │   ├── activity_main.xml        # 主頁布局
            │   └── activity_device_list.xml # 設備列表布局
            ├── drawable/
            │   ├── bg_page.xml              # 頁面深色漸變背景
            │   ├── bg_card.xml              # 卡片背景
            │   ├── bg_input.xml              # 輸入框背景
            │   ├── btn_direction_bg.xml      # 方向鍵圓形按鈕
            │   ├── btn_stop_bg.xml           # STOP 紅色按鈕
            │   ├── btn_primary_bg.xml        # 主按鈕（霓虹青漸變）
            │   ├── btn_secondary_bg.xml      # 次要按鈕
            │   └── ic_launcher.xml           # APP 圖標
            └── values/
                ├── colors.xml                # 主題配色（霓虹青）
                ├── dimens.xml                # 尺寸變量
                ├── strings.xml               # 字符串
                └── themes.xml                # 主題樣式
```

---

## 🎨 設計風格
- **霓虹青科技感**深色主題（與之前小程序保持一致）
- 主色 `#00E0FF` 霓虹青，背景深藍漸變 `#0B1020 → #131A2E`
- 方向鍵採用雷霆大按鈕（120dp），按下時填充霓虹青並保持高對比度
- 全 Material Design 圓角卡片 + 半透明霓虹青邊框

---

## 📲 使用流程

1. 打開手機藍牙
2. 在手機藍牙設置中**先配對** JDY-31 模組（密碼通常為 `1234` 或 `0000`），這一步可選但建議做
3. 打開 APP → 點「連接設備」 → 列表中選擇 JDY-31
4. 連接成功後，APP 頂部顯示「已連接」綠色狀態
5. 按方向鍵即可遙控小車
6. 按緊急停車按鈕可立即停止

---

## 🔧 技術要點

### 藍牙 SPP 連接
- 使用標準 SPP UUID: `00001101-0000-1000-8000-00805F9B34FB`
- JDY-31、HC-05、HC-06 等經典藍牙透傳模組出廠即用此 UUID
- 連接在獨立 `ConnectThread` 中進行，自動重試 3 次
- 收發在獨立 `ConnectedThread` 中進行，UI 不阻塞

### 權限處理
- **Android 12+（API 31+）**：使用 `BLUETOOTH_SCAN` 和 `BLUETOOTH_CONNECT` 細粒度權限
- **Android 11 及以下**：使用 `ACCESS_FINE_LOCATION` 位置權限（掃描需要）
- 動態請求權限 + 優雅降級

### 線程安全
- 藍牙回調通過 `Handler(Looper.getMainLooper())` 投遞到主線程更新 UI
- `@Volatile` 修飾共享狀態
- `@Synchronized` 保護線程創建/銷毀

---

## ❓ 常見問題

| 症狀 | 解決方案 |
|------|---------|
| 掃描不到設備 | 手機藍牙未開 / 未給位置權限 / JDY-31 未上電 |
| 連接失敗（重試 3 次失敗）| 模組未上電 / 信號弱 / 距離太遠 / 已被其他手機連接 |
| 連上但不動 | 固件端 OLED 檢查 RX 是否收到字節；檢查 TX/RX 交叉接線；檢查波特率 9600 |
| APP 閃退 | 確認手機 Android 5.0+（API 21+） |
| 安裝時提示「未知來源」 | 手機「設置 → 應用管理 → 允許此來源安裝」 |

---

## 📝 開發備註

- **minSdk 21**（Android 5.0）：覆蓋 99% 設備
- **targetSdk 34**（Android 14）：滿足 Google Play 上架要求
- **ViewBinding** 已啟用，類型安全訪問 View
- **Material Components** 主題，提供現代化視覺風格
