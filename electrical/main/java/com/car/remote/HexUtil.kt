package com.car.remote

/** 字節與十六進制字符串轉換工具 */
object HexUtil {

    fun toHex(bytes: ByteArray, separator: String = " "): String {
        if (bytes.isEmpty()) return ""
        val sb = StringBuilder(bytes.size * 3)
        for ((i, b) in bytes.withIndex()) {
            if (i > 0) sb.append(separator)
            sb.append(String.format("%02X", b.toInt() and 0xFF))
        }
        return sb.toString()
    }

    fun toHex(b: Int): String = String.format("%02X", b and 0xFF)

    fun toAsciiString(bytes: ByteArray): String {
        return String(bytes, Charsets.US_ASCII)
    }

    /** 格式化 RSSI 為可讀字符串 */
    fun formatRssi(rssi: Int): String = "${rssi} dBm"
}
