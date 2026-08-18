package com.car.remote

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager as SystemBtManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.content.Intent
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID

/**
 * 經典藍牙 SPP 管理器（單例）
 *
 * 設計說明：
 *  - 使用 SPP UUID: 00001101-0000-1000-8000-00805F9B34FB
 *    這是經典藍牙 SPP 服務的標準 UUID，JDY-31 / HC-05 / HC-06 等
 *    經典藍牙透傳模組出廠即使用此 UUID
 *  - 連接在獨立線程中進行，避免阻塞 UI
 *  - 數據收發在獨立線程中進行，確保實時性
 *  - 通過 Listener 回調通知 UI 狀態變化與數據接收
 */
class BluetoothManager private constructor() {

    // ============================================================
    // 狀態定義
    // ============================================================
    enum class State {
        NONE,       // 未初始化
        IDLE,       // 已初始化，未連接
        CONNECTING, // 連接中
        CONNECTED,  // 已連接（GATT Socket 打開）
        ERROR       // 錯誤
    }

    interface Listener {
        fun onStateChanged(state: State, device: BluetoothDevice?)
        fun onDataReceived(data: ByteArray)
        fun onError(message: String)
        fun onSent(bytes: ByteArray)
    }

    companion object {
        private const val TAG = "BluetoothManager"

        @Volatile
        private var INSTANCE: BluetoothManager? = null

        @JvmStatic
        fun getInstance(): BluetoothManager =
            INSTANCE ?: synchronized(this) {
                INSTANCE ?: BluetoothManager().also { INSTANCE = it }
            }

        /** SPP 經典藍牙標準 UUID */
        val UUID_SPP: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    }

    // ============================================================
    // 字段
    // ============================================================
    private var context: Context? = null
    private val handler = Handler(Looper.getMainLooper())

    private var bluetoothAdapter: BluetoothAdapter? = null
    private var connectThread: ConnectThread? = null
    private var connectedThread: ConnectedThread? = null

    @Volatile
    private var currentState: State = State.NONE
    private var currentDevice: BluetoothDevice? = null

    private val listeners = mutableSetOf<Listener>()

    // ============================================================
    // 初始化
    // ============================================================
    fun init(ctx: Context) {
        if (this.context != null) return
        this.context = ctx.applicationContext
        val bm = ctx.getSystemService(Context.BLUETOOTH_SERVICE) as? SystemBtManager
        bluetoothAdapter = bm?.adapter
        setState(if (bluetoothAdapter != null) State.IDLE else State.NONE, null)
    }

    fun addListener(l: Listener) { listeners.add(l) }
    fun removeListener(l: Listener) { listeners.remove(l) }

    fun getAdapter(): BluetoothAdapter? = bluetoothAdapter
    fun getState(): State = currentState
    fun getDevice(): BluetoothDevice? = currentDevice

    @SuppressLint("MissingPermission")
    fun isBluetoothEnabled(): Boolean = bluetoothAdapter?.isEnabled == true

    /** 請求系統開啟藍牙（跳轉系統設置） */
    fun requestEnable(ctx: Context) {
        val intent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        ctx.startActivity(intent)
    }

    // ============================================================
    // 連接 / 斷開
    // ============================================================
    @SuppressLint("MissingPermission")
    fun connect(device: BluetoothDevice) {
        Log.i(TAG, "connect to ${device.name}(${device.address})")
        cancelConnectThread()
        cancelConnectedThread()
        currentDevice = device
        setState(State.CONNECTING, device)
        connectThread = ConnectThread(device).also { it.start() }
    }

    fun disconnect() {
        Log.i(TAG, "disconnect")
        cancelConnectThread()
        cancelConnectedThread()
        currentDevice = null
        setState(State.IDLE, null)
    }

    /** 完全釋放資源（APP 退出時調用） */
    fun release() {
        disconnect()
        listeners.clear()
        context = null
        setState(State.NONE, null)
    }

    // ============================================================
    // 發送數據
    // ============================================================
    fun write(data: ByteArray): Boolean {
        val ct = connectedThread ?: run {
            notifyError("尚未連接藍牙")
            return false
        }
        if (!ct.write(data)) return false
        handler.post { listeners.forEach { it.onSent(data) } }
        return true
    }

    /** 發送單條指令（按 useAscii 決定發 HEX 還是 ASCII） */
    fun sendCommand(cmd: CarCommand, useAscii: Boolean): Boolean {
        return write(CarCommand.payload(cmd, useAscii))
    }

    /** 緊急停車：直接發 0x00，不論模式 */
    fun emergencyStop(): Boolean = write(byteArrayOf(0x00))

    // ============================================================
    // 內部線程：連接
    // ============================================================
    @SuppressLint("MissingPermission")
    private inner class ConnectThread(private val device: BluetoothDevice) : Thread() {
        private var socket: BluetoothSocket? = null
        private val btStateIdle = BluetoothManager.State.IDLE

        init {
            try {
                socket = device.createRfcommSocketToServiceRecord(UUID_SPP)
            } catch (e: IOException) {
                Log.e(TAG, "createRfcommSocketToServiceRecord 失敗", e)
                notifyError("Socket 創建失敗: ${e.message}")
            }
        }

        override fun run() {
            bluetoothAdapter?.cancelDiscovery()
            var retry = 0
            while (retry < 3 && socket != null) {
                try {
                    socket!!.connect()
                    synchronized(this@BluetoothManager) {
                        connectThread = null
                    }
                    onConnected(socket!!, device)
                    return
                } catch (e: IOException) {
                    retry++
                    Log.w(TAG, "連接失敗(第 ${retry} 次): ${e.message}")
                    SystemClock.sleep(300)
                }
            }
            // 重試 3 次都失敗
            try { socket?.close() } catch (_: IOException) {}
            notifyError("連接失敗（請確認設備已開機並處於可連接狀態）")
            handler.post {
                synchronized(this@BluetoothManager) { connectThread = null }
                setState(btStateIdle, device)
            }
        }

        fun cancel() {
            try { socket?.close() } catch (_: IOException) {}
        }
    }

    // ============================================================
    // 內部線程：已連接後的收發
    // ============================================================
    private inner class ConnectedThread(
        private val socket: BluetoothSocket,
        private val device: BluetoothDevice
    ) : Thread() {

        private var inputStream: InputStream? = null
        private var outputStream: OutputStream? = null
        @Volatile private var isRunning = true
        private val btStateIdle = BluetoothManager.State.IDLE

        init {
            try {
                inputStream = socket.inputStream
                outputStream = socket.outputStream
            } catch (e: IOException) {
                Log.e(TAG, "獲取流失敗", e)
                notifyError("獲取 IO 流失敗: ${e.message}")
            }
        }

        override fun run() {
            val buffer = ByteArray(1024)
            while (isRunning) {
                try {
                    val bytes = inputStream?.read(buffer) ?: -1
                    if (bytes > 0) {
                        val data = buffer.copyOfRange(0, bytes)
                        handler.post { listeners.forEach { it.onDataReceived(data) } }
                    } else if (bytes == -1) {
                        // 對端關閉
                        break
                    }
                } catch (e: IOException) {
                    Log.e(TAG, "讀取失敗: ${e.message}")
                    break
                }
            }
            // 連接斷開
            handler.post {
                notifyError("連接已斷開")
                synchronized(this@BluetoothManager) { connectedThread = null }
                setState(btStateIdle, device)
            }
        }

        fun write(data: ByteArray): Boolean {
            try {
                val os = outputStream ?: run {
                    Log.e(TAG, "發送失敗: outputStream 為空")
                    notifyError("發送失敗: 未獲取輸出流")
                    return false
                }
                os.write(data)
                os.flush()
                return true
            } catch (e: IOException) {
                Log.e(TAG, "發送失敗: ${e.message}")
                notifyError("發送失敗: ${e.message}")
                return false
            }
        }

        fun cancel() {
            isRunning = false
            try { socket.close() } catch (_: IOException) {}
        }
    }

    @SuppressLint("MissingPermission")
    private fun onConnected(socket: BluetoothSocket, device: BluetoothDevice) {
        Log.i(TAG, "已連接到 ${device.name}")
        connectedThread?.cancel()
        connectedThread = ConnectedThread(socket, device).also { it.start() }
        handler.post {
            setState(State.CONNECTED, device)
        }
    }

    // ============================================================
    // 工具方法
    // ============================================================
    private fun setState(state: State, device: BluetoothDevice?) {
        currentState = state
        handler.post { listeners.forEach { it.onStateChanged(state, device) } }
    }

    private fun notifyError(msg: String) {
        handler.post { listeners.forEach { it.onError(msg) } }
    }

    @Synchronized
    private fun cancelConnectThread() {
        connectThread?.cancel()
        connectThread = null
    }

    @Synchronized
    private fun cancelConnectedThread() {
        connectedThread?.cancel()
        connectedThread = null
    }
}
