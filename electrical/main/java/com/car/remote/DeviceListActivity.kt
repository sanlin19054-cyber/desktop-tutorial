package com.car.remote

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.car.remote.databinding.ActivityDeviceListBinding

/**
 * 藍牙設備選擇頁：
 *  1) 顯示已配對設備
 *  2) 啟動掃描，發現新設備
 *  3) 點擊設備 → 返回給 MainActivity 進行連接
 */
class DeviceListActivity : AppCompatActivity() {

    private lateinit var binding: ActivityDeviceListBinding
    private val devices = mutableListOf<BluetoothDevice>()
    private lateinit var adapter: DeviceAdapter

    /** 掃描結果 + 配對狀態變化 廣播接收器 */
    private val receiver = object : BroadcastReceiver() {
        @SuppressLint("MissingPermission")
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                BluetoothDevice.ACTION_FOUND -> {
                    val device = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
                    } else {
                        @Suppress("DEPRECATION")
                        intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                    }
                    device?.let {
                        if (it.address !in devices.map { d -> d.address }) {
                            devices.add(it)
                            adapter.notifyItemInserted(devices.size - 1)
                        }
                    }
                }
                BluetoothAdapter.ACTION_DISCOVERY_STARTED -> {
                    binding.btnScan.text = "停止掃描"
                    binding.txtStatus.text = "掃描中..."
                }
                BluetoothAdapter.ACTION_DISCOVERY_FINISHED -> {
                    binding.btnScan.text = "開始掃描"
                    binding.txtStatus.text = "掃描完成（共 ${devices.size} 個設備）"
                }
            }
        }
    }

    /** 藍牙權限請求 */
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        val allGranted = result.values.all { it }
        if (allGranted) {
            loadBondedDevices()
            startScan()
        } else {
            Toast.makeText(this, "需要藍牙權限才能掃描設備", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityDeviceListBinding.inflate(layoutInflater)
        setContentView(binding.root)

        adapter = DeviceAdapter(devices) { device -> returnDevice(device) }
        binding.recyclerDevices.layoutManager = LinearLayoutManager(this)
        binding.recyclerDevices.adapter = adapter

        binding.btnScan.setOnClickListener {
            val adapter = BluetoothManager.getInstance().getAdapter()
            if (adapter == null) {
                Toast.makeText(this, "設備不支持藍牙", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            if (!adapter.isEnabled) {
                BluetoothManager.getInstance().requestEnable(this)
                return@setOnClickListener
            }
            if (adapter.isDiscovering) {
                adapter.cancelDiscovery()
            } else {
                if (checkPermissions()) startScan()
            }
        }

        // 註冊廣播
        val filter = IntentFilter().apply {
            addAction(BluetoothDevice.ACTION_FOUND)
            addAction(BluetoothAdapter.ACTION_DISCOVERY_STARTED)
            addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            registerReceiver(receiver, filter)
        }

        // 初始化：檢查權限 → 加載已配對設備
        if (checkPermissions()) {
            loadBondedDevices()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        try { unregisterReceiver(receiver) } catch (_: Exception) {}
        BluetoothManager.getInstance().getAdapter()?.cancelDiscovery()
    }

    @SuppressLint("MissingPermission")
    private fun loadBondedDevices() {
        val btAdapter = BluetoothManager.getInstance().getAdapter() ?: return
        devices.clear()
        devices.addAll(btAdapter.bondedDevices)
        this@DeviceListActivity.adapter.notifyDataSetChanged()
        binding.txtStatus.text = "已配對設備：${devices.size} 個（點擊「開始掃描」查找更多）"
    }

    @SuppressLint("MissingPermission")
    private fun startScan() {
        val btAdapter = BluetoothManager.getInstance().getAdapter() ?: return
        if (btAdapter.isDiscovering) btAdapter.cancelDiscovery()
        devices.clear()
        loadBondedDevices()
        // 開始掃描（需 12 秒左右，系統控制）
        if (ActivityCompat.checkSelfPermission(
                this, Manifest.permission.ACCESS_FINE_LOCATION
            ) == PackageManager.PERMISSION_GRANTED
        ) {
            btAdapter.startDiscovery()
        } else {
            Toast.makeText(this, "需要位置權限才能掃描藍牙", Toast.LENGTH_LONG).show()
        }
    }

    private fun checkPermissions(): Boolean {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.ACCESS_FINE_LOCATION
            )
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        val need = permissions.filter {
            ActivityCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (need.isEmpty()) return true
        permissionLauncher.launch(need.toTypedArray())
        return false
    }

    private fun returnDevice(device: BluetoothDevice) {
        val data = Intent().apply {
            putExtra(EXTRA_DEVICE_ADDRESS, device.address)
        }
        setResult(Activity.RESULT_OK, data)
        finish()
    }

    // ============================================================
    // 設備列表 Adapter
    // ============================================================
    @SuppressLint("MissingPermission")
    private inner class DeviceAdapter(
        private val items: List<BluetoothDevice>,
        private val onClick: (BluetoothDevice) -> Unit
    ) : RecyclerView.Adapter<DeviceAdapter.VH>() {

        inner class VH(val container: LinearLayout) : RecyclerView.ViewHolder(container)

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            val container = LinearLayout(this@DeviceListActivity).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(48, 36, 48, 36)
                layoutParams = ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                )
                setBackgroundResource(R.drawable.bg_card)
            }
            val name = TextView(this@DeviceListActivity).apply {
                textSize = 16f
                setTextColor(getColor(R.color.text_primary))
            }
            val addr = TextView(this@DeviceListActivity).apply {
                textSize = 12f
                setTextColor(getColor(R.color.text_secondary))
            }
            container.addView(name)
            container.addView(addr)
            container.tag = name to addr
            return VH(container)
        }

        override fun onBindViewHolder(holder: VH, position: Int) {
            val (nameTv, addrTv) = holder.container.tag as Pair<TextView, TextView>
            val device = items[position]
            val name = try { device.name } catch (_: SecurityException) { "未知設備" } ?: "未知設備"
            val addr = device.address
            nameTv.text = if (name.isBlank() || name == "未知設備") "未命名設備 ($addr)" else name
            addrTv.text = addr
            holder.container.setOnClickListener { onClick(device) }
        }

        override fun getItemCount(): Int = items.size
    }

    companion object {
        const val EXTRA_DEVICE_ADDRESS = "extra_device_address"
    }
}
