using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Android.Bluetooth;
using Android.Bluetooth.LE;
using Android.Content;
using Android.Runtime;
using Android.Util;

namespace BlueWiFi
{
    /// <summary>
    /// 低功耗蓝牙
    /// + 蓝牙适配器的开启/关闭状态
    /// + 扫描BLE设备
    /// + TODO: GATT读写
    /// </summary>
    class StationBLE
    {
        private readonly ContextWrapper m_ctx;
        private readonly BluetoothAdapter m_adapter;
        private readonly StationBLEBroadcastRecver m_receiver;
        private readonly List<BluetoothDevice> m_deviceScanned = new List<BluetoothDevice>();
        private Task m_taskWaitScanStop;

        //private 
        public StationBLE(ContextWrapper ctx)
        {
            m_ctx = ctx;
            var btMgr = (BluetoothManager)ctx.GetSystemService(Context.BluetoothService);
            m_adapter = btMgr.Adapter;
            m_receiver = new StationBLEBroadcastRecver(this, ctx);
        }

        public void Init()
        {
            m_receiver.Register();
        }

        public void Uninit()
        {
            m_receiver.Unregister();
        }

        // 蓝牙适配器设定的本机名称
        public string StationName => m_adapter.Name;

        // 蓝牙适配器是否开启
        public bool IsBTEnabled => m_adapter.IsEnabled;

        // 蓝牙适配器开启状态通知
        public Action<bool> OnBTEnableChanged { get; set; }

        // 按名称获取先前扫描到的蓝牙设备
        public BluetoothDevice GetScannedDevice(string name)
        {
            return m_deviceScanned.Find(x => x?.Name == name);
        }

        // 扫描BLE设备
        public async Task<IReadOnlyList<BluetoothDevice>> ScanAsync(TimeSpan tsDuration, int nMaxCount, string strPattern, bool bRegex)
        {
            if (m_taskWaitScanStop == null) {
                m_deviceScanned.Clear();

                Regex rgxName = null;
                // 不填代表扫描所有设备,包括没有名称,只有地址的
                bool bScanForAll = String.IsNullOrEmpty(strPattern);
                if (!bScanForAll && bRegex) rgxName = new Regex(strPattern, RegexOptions.Compiled);

                var evStop = new ManualResetEvent(false);
                var scanner = m_adapter.BluetoothLeScanner;
                var settings = new ScanSettings.Builder().SetScanMode(Android.Bluetooth.LE.ScanMode.LowPower).Build();

                // 剔重
                var devFound = new HashSet<string>(nMaxCount);
                Action<BluetoothDevice> actionAddDevice = (BluetoothDevice dev) => {
                    if (!devFound.Contains(dev.Address)) {
                        devFound.Add(dev.Address);
                        m_deviceScanned.Add(dev);
                    }
                };

                var cb = new StationBLEScanCallback(
                    (type, result) => {
                        if (m_deviceScanned.Count < nMaxCount) {
                            if (bScanForAll) {
                                actionAddDevice(result.Device);
                            }
                            else if (result.Device.Name != null) {
                                if (rgxName != null && rgxName.IsMatch(result.Device.Name)) {
                                    actionAddDevice(result.Device);
                                }
                                else if (result.Device.Name == strPattern) {
                                    actionAddDevice(result.Device);
                                }
                            }
                        }

                        if (m_deviceScanned.Count >= nMaxCount) {
                            // the max count reached
                            evStop.Set();
                        }
                    },
                    (failure) => {
                        Log.Error(Ref.TAG, $"scan bluetooth-le devices failed: {failure}");
                        evStop.Set();
                    });

                scanner.StartScan(null, settings, cb);

                m_taskWaitScanStop = new Task(() => {
                    evStop.WaitOne(tsDuration);
                    scanner.StopScan(cb);
                });
                m_taskWaitScanStop.Start();
            }

            await m_taskWaitScanStop;
            m_taskWaitScanStop = null;

            return m_deviceScanned;
        }

        public StationOperator CreateGattOperation(string name)
        {
            var dev = GetScannedDevice(name);
            if (dev == null) return null;
            var gattOp = new StationOperator(m_ctx, dev);
            return gattOp;
        }
    }

    class StationBLEBroadcastRecver : BroadcastReceiver
    {
        private readonly StationBLE m_station;
        private readonly ContextWrapper m_ctx;

        public StationBLEBroadcastRecver(StationBLE station, ContextWrapper ctx)
        {
            m_station = station;
            m_ctx = ctx;
        }

        public void Register()
        {
            m_ctx.RegisterReceiver(this, new IntentFilter(BluetoothAdapter.ActionStateChanged));
        }

        public void Unregister()
        {
            m_ctx.UnregisterReceiver(this);
        }

        public override void OnReceive(Context context, Intent intent)
        {
            State state = (State)intent.GetIntExtra(BluetoothAdapter.ExtraState, BluetoothAdapter.Error);
            if (state == State.On || state == State.Off) {
                // we're not interested in turning on/off state
                m_station.OnBTEnableChanged?.Invoke(state == State.On);
            }
        }
    }
    class StationBLEScanCallback : ScanCallback
    {
        private readonly Action<ScanCallbackType, ScanResult> m_actionResult;
        private readonly Action<ScanFailure> m_actionFailed;
        public StationBLEScanCallback(Action<ScanCallbackType, ScanResult> onResult, Action<ScanFailure> onFailed)
        {
            m_actionResult = onResult;
            m_actionFailed = onFailed;
        }

        public override void OnScanResult([GeneratedEnum] ScanCallbackType callbackType, ScanResult result)
        {
            base.OnScanResult(callbackType, result);
            m_actionResult(callbackType, result);
        }

        public override void OnBatchScanResults(IList<ScanResult> results)
        {
            base.OnBatchScanResults(results);
            foreach (var result in results)
                m_actionResult(0, result);
        }

        public override void OnScanFailed([GeneratedEnum] ScanFailure errorCode)
        {
            base.OnScanFailed(errorCode);

            // ScanFailure.AlreadyStarte is actually a normal behavior, always report this
            if (errorCode != ScanFailure.AlreadyStarted) m_actionFailed(errorCode);
        }
    }
}