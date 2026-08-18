package com.car.remote

import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothDevice
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.text.InputType
import android.view.MotionEvent
import android.view.View
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import com.car.remote.databinding.ActivityMainBinding

/**
 * 遙控主頁（橫版）：
 *  - 頂部狀態列（設備名/已發送次數/上次發送/連接按鈕）
 *  - 左側 5 向方向鍵（前/後/左/右 + STOP，按住式控制含心跳）
 *  - 右側 4 個模式快捷按鈕 + 手動調試
 *
 * 改進點：
 *  1. 方向鍵按住時以 ~80ms 心跳持續重發指令，避免「輕點被 STOP 覆蓋」
 *  2. 鬆開後補發數次 STOP，防止 STOP 在傳輸過程中丟失
 *  3. 「已發送次數」加入消抖：同指令在 150ms 內不重複計數
 */
class MainActivity : AppCompatActivity(), BluetoothManager.Listener {

    private lateinit var binding: ActivityMainBinding

    // ---- 計數與顯示 ----
    private var sentCount = 0
    private var lastSent: ByteArray = ByteArray(0)
    private var useAscii = false  // false=HEX, true=ASCII

    // ---- 消抖：避免同一指令在短時間內被重複計數（觸控螢幕雜訊事件） ----
    private var lastCountedTimeMs: Long = 0
    private val countDebounceMs = 150L

    // ---- 心跳 / 安全 STOP 排程 ----
    private val handler = Handler(Looper.getMainLooper())
    private var activeRunnable: Runnable? = null

    companion object {
        /** 方向鍵按住時，每隔多久重發一次指令（需短於固件主循環週期，確保 RxData 最新） */
        private const val HEARTBEAT_INTERVAL_MS = 80L

        /** 鬆開後補發 STOP 的次數與間隔，防丟失 */
        private const val STOP_SAFETY_REPEAT = 3
        private const val STOP_SAFETY_INTERVAL_MS = 40L
    }

    /** 從設備列表返回結果 */
    private val deviceListLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            val addr = result.data?.getStringExtra(DeviceListActivity.EXTRA_DEVICE_ADDRESS)
            if (addr != null) {
                val device = BluetoothManager.getInstance().getAdapter()?.getRemoteDevice(addr)
                if (device != null) {
                    showStatus("連接中：${device.name ?: addr}")
                    BluetoothManager.getInstance().connect(device)
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // 初始化藍牙管理器
        BluetoothManager.getInstance().init(applicationContext)
        BluetoothManager.getInstance().addListener(this)

        // 連接按鈕 → 打開設備選擇頁
        binding.btnConnect.setOnClickListener {
            if (BluetoothManager.getInstance().isBluetoothEnabled()) {
                deviceListLauncher.launch(Intent(this, DeviceListActivity::class.java))
            } else {
                Toast.makeText(this, "請先開啟藍牙", Toast.LENGTH_SHORT).show()
                BluetoothManager.getInstance().requestEnable(this)
            }
        }

        // 斷開按鈕
        binding.btnDisconnect.setOnClickListener {
            BluetoothManager.getInstance().disconnect()
        }

        // ===== 5 向方向鍵：按住發送對應指令 + 心跳重發，鬆開發 STOP + 安全補發 =====
        setupDirectionButton(binding.btnUp,    CarCommand.FORWARD)
        setupDirectionButton(binding.btnDown,  CarCommand.BACKWARD)
        setupDirectionButton(binding.btnLeft,   CarCommand.LEFT)
        setupDirectionButton(binding.btnRight,  CarCommand.RIGHT)
        // STOP 是單擊
        binding.btnStop.setOnClickListener {
            sendAndRecord(CarCommand.STOP)
        }

        // ===== 4 個模式快捷按鈕（單擊觸發） =====
        binding.btnAvoid.setOnClickListener  { sendAndRecord(CarCommand.AVOID) }
        binding.btnTrack.setOnClickListener  { sendAndRecord(CarCommand.TRACK) }
        binding.btnLedOn.setOnClickListener { sendAndRecord(CarCommand.LED_ON) }
        binding.btnLedOff.setOnClickListener{ sendAndRecord(CarCommand.LED_OFF) }

        // ===== 模式切換：HEX / ASCII =====
        binding.switchMode.isChecked = useAscii
        binding.switchMode.setOnCheckedChangeListener { _, checked ->
            useAscii = checked
            updateModeLabel()
        }
        updateModeLabel()

        // ===== 手動輸入發送 =====
        binding.btnSend.setOnClickListener {
            val input = binding.editInput.text.toString()
            val bytes = CarCommand.parseManual(input, useAscii) ?: run {
                Toast.makeText(this, "輸入格式錯誤", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            if (BluetoothManager.getInstance().write(bytes)) {
                recordSent(bytes)
                Toast.makeText(this, "已發送：${HexUtil.toHex(bytes)}", Toast.LENGTH_SHORT).show()
            }
        }

        // ===== 緊急停車 =====
        binding.btnEmergencyStop.setOnClickListener {
            // 先停掉任何進行中的心跳，避免心跳繼續發出非 STOP 指令
            cancelActive()
            if (BluetoothManager.getInstance().emergencyStop()) {
                recordSent(byteArrayOf(0x00))
                startSafetyStop()
            }
        }

        updateStats()
    }

    override fun onDestroy() {
        super.onDestroy()
        cancelActive()
        BluetoothManager.getInstance().removeListener(this)
    }

    // ============================================================
    // 方向鍵：按住式控制
    //  - ACTION_DOWN：發送指令並計數、啟動心跳（每 80ms 重發）
    //  - ACTION_UP/CANCEL：停止心跳、發送 STOP 並計數、啟動安全 STOP 補發
    // ============================================================
    @SuppressLint("ClickableViewAccessibility")
    private fun setupDirectionButton(view: View, cmd: CarCommand) {
        view.setOnTouchListener { v, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    // 禁止外層 ScrollView 攔截 touch 事件，確保按住期間心跳不中斷
                    v.parent?.requestDisallowInterceptTouchEvent(true)
                    v.isPressed = true
                    cancelActive()
                    sendAndRecord(cmd)       // 計數 + 發送
                    startHeartbeat(cmd)      // 持續重發，確保固件 RxData 始終為當前指令
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    v.parent?.requestDisallowInterceptTouchEvent(false)
                    v.isPressed = false
                    cancelActive()
                    sendAndRecord(CarCommand.STOP)   // 計數 + 發送 STOP
                    startSafetyStop()                 // 補發數次 STOP，防丟失
                }
            }
            true
        }
    }

    /** 僅發送藍牙字節（不計數），供心跳與安全 STOP 使用 */
    private fun transmitOnly(cmd: CarCommand) {
        BluetoothManager.getInstance().sendCommand(cmd, useAscii)
    }

    /** 發送並計數（含消抖：同一指令在 150ms 內不重複計數） */
    private fun sendAndRecord(cmd: CarCommand) {
        if (!BluetoothManager.getInstance().sendCommand(cmd, useAscii)) return
        recordSent(CarCommand.payload(cmd, useAscii))
    }

    /**
     * 記錄一次成功發送並更新 UI。
     * 消抖邏輯：若與上次發送內容相同且時間在 [countDebounceMs] 內，則不重複計數。
     * 這樣心跳重發不會灌水計數器，但用戶每次按下/鬆開都會被計入。
     */
    private fun recordSent(bytes: ByteArray) {
        val now = SystemClock.elapsedRealtime()
        val isDuplicate = lastSent.contentEquals(bytes) &&
                          (now - lastCountedTimeMs < countDebounceMs)
        lastSent = bytes
        if (!isDuplicate) {
            sentCount++
            lastCountedTimeMs = now
        }
        updateStats()
    }

    /** 啟動心跳：每 HEARTBEAT_INTERVAL_MS 重發同一指令 */
    private fun startHeartbeat(cmd: CarCommand) {
        cancelActive()
        val r = object : Runnable {
            override fun run() {
                transmitOnly(cmd)
                handler.postDelayed(this, HEARTBEAT_INTERVAL_MS)
            }
        }
        activeRunnable = r
        handler.postDelayed(r, HEARTBEAT_INTERVAL_MS)
    }

    /** 鬆開後補發 STOP_SAFETY_REPEAT 次 STOP，確保 STOP 指令送達 */
    private fun startSafetyStop() {
        cancelActive()
        var remaining = STOP_SAFETY_REPEAT
        val r = object : Runnable {
            override fun run() {
                if (remaining <= 0) return
                transmitOnly(CarCommand.STOP)
                remaining--
                if (remaining > 0) {
                    handler.postDelayed(this, STOP_SAFETY_INTERVAL_MS)
                }
            }
        }
        activeRunnable = r
        handler.post(r)
    }

    /** 取消當前心跳或安全 STOP 排程 */
    private fun cancelActive() {
        activeRunnable?.let { handler.removeCallbacks(it) }
        activeRunnable = null
    }

    // ============================================================
    // BluetoothManager.Listener 回調
    // ============================================================
    override fun onStateChanged(state: BluetoothManager.State, device: BluetoothDevice?) {
        runOnUiThread {
            val stateText = when (state) {
                BluetoothManager.State.NONE       -> "未初始化"
                BluetoothManager.State.IDLE       -> "未連接"
                BluetoothManager.State.CONNECTING -> "連接中..."
                BluetoothManager.State.CONNECTED  -> "已連接"
                BluetoothManager.State.ERROR      -> "錯誤"
            }
            binding.txtConnState.text = stateText
            binding.txtDeviceName.text = device?.name ?: "尚未選擇設備"
            binding.txtDeviceAddr.text = device?.address ?: "—"

            val ready = state == BluetoothManager.State.CONNECTED
            binding.btnUp.isEnabled = ready
            binding.btnDown.isEnabled = ready
            binding.btnLeft.isEnabled = ready
            binding.btnRight.isEnabled = ready
            binding.btnStop.isEnabled = ready
            binding.btnAvoid.isEnabled = ready
            binding.btnTrack.isEnabled = ready
            binding.btnLedOn.isEnabled = ready
            binding.btnLedOff.isEnabled = ready
            binding.btnSend.isEnabled = ready
            binding.btnEmergencyStop.isEnabled = ready
            binding.btnDisconnect.isEnabled = ready
            binding.btnDisconnect.visibility = if (ready) View.VISIBLE else View.GONE
            binding.btnConnect.visibility    = if (ready) View.GONE else View.VISIBLE

            if (state == BluetoothManager.State.CONNECTED) {
                Toast.makeText(this, "已連接到 ${device?.name}", Toast.LENGTH_SHORT).show()
            }
            // 連線中斷時，停掉任何殘留的心跳排程，避免對斷開的 socket 寫入
            if (state == BluetoothManager.State.IDLE ||
                state == BluetoothManager.State.ERROR) {
                cancelActive()
            }
        }
    }

    override fun onDataReceived(data: ByteArray) {
        runOnUiThread {
            binding.txtRxBuf.text =
                "${HexUtil.toHex(data)}   (ASCII: ${HexUtil.toAsciiString(data)})"
        }
    }

    override fun onError(message: String) {
        runOnUiThread {
            Toast.makeText(this, message, Toast.LENGTH_LONG).show()
            showStatus(message)
        }
    }

    override fun onSent(bytes: ByteArray) {
        runOnUiThread {
            // sentCount 已在 sendAndRecord / recordSent 中更新
        }
    }

    // ============================================================
    // UI 工具
    // ============================================================
    private fun updateModeLabel() {
        binding.txtModeLabel.text = if (useAscii) "ASCII（發 '0'~'8'）" else "HEX（發 0x00~0x08）"
        binding.editInput.hint = if (useAscii) "輸入字符，例如 1" else "輸入 HEX，例如 01 或 01 02"
        binding.editInput.inputType = if (useAscii) InputType.TYPE_CLASS_TEXT
                                     else InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
    }

    private fun updateStats() {
        val display = "${HexUtil.toHex(lastSent)} (${HexUtil.toAsciiString(lastSent)})"
        binding.txtSentCount.text = sentCount.toString()
        binding.txtLastSent.text = display
        binding.txtTxLastSent?.text = display
    }

    private fun showStatus(msg: String) {
        binding.txtConnState.text = msg
    }
}
