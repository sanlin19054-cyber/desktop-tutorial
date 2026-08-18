package com.car.remote

/**
 * 與 STM32 智能車固件對應的指令協議表
 * 固件 switch(RxData)：0x00 停止 / 0x01 前 / 0x02 後 / 0x03 左 / 0x04 右
 *                  / 0x05 避障 / 0x06 循跡 / 0x07 開燈 / 0x08 關燈
 * 注：固件同時兼容 ASCII '0'~'8'（0x30~0x38），自動映射到 0x00~0x08
 */
enum class CarCommand(val code: Int, val ascii: Char, val label: String, val hint: String) {
    STOP    (0x00, '0', "停止", "立即停止所有電機"),
    FORWARD (0x01, '1', "前進", "雙輪正轉 (50, 50)"),
    BACKWARD(0x02, '2', "後退", "雙輪反轉 (-40, -40)"),
    LEFT    (0x03, '3', "左轉", "左反右正 (-40, 40)"),
    RIGHT   (0x04, '4', "右轉", "左正右反 (40, -40)"),
    AVOID   (0x05, '5', "避障", "舵機掃描 + 避障運行"),
    TRACK   (0x06, '6', "循跡", "紅外循跡模式"),
    LED_ON  (0x07, '7', "開燈", "打開 LED 指示燈"),
    LED_OFF (0x08, '8', "關燈", "關閉 LED 指示燈");

    companion object {
        /** 根據發送模式取得要發送的字節數組 */
        fun payload(cmd: CarCommand, useAscii: Boolean): ByteArray {
            return if (useAscii) {
                byteArrayOf(cmd.ascii.code.toByte())
            } else {
                byteArrayOf(cmd.code.toByte())
            }
        }

        /** 解析用戶手動輸入的字節序列
         *  useAscii=true 時：輸入字符串逐字符轉 byte
         *  useAscii=false 時：解析 HEX（如 "01 02" 或 "01,02" 或 "0x01"） */
        fun parseManual(input: String, useAscii: Boolean): ByteArray? {
            val trimmed = input.trim()
            if (trimmed.isEmpty()) return null
            return if (useAscii) {
                trimmed.toByteArray(Charsets.US_ASCII)
            } else {
                val cleaned = trimmed.replace(Regex("0x", RegexOption.IGNORE_CASE), "")
                    .replace(Regex("[\\s,;]+"), "")
                if (cleaned.isEmpty() || cleaned.length % 2 != 0) return null
                try {
                    cleaned.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
                } catch (e: NumberFormatException) {
                    null
                }
            }
        }
    }
}
