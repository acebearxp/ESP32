using System;
using System.Threading;
using System.Threading.Tasks;
using Android.Bluetooth;
using Android.Content;
using Android.Runtime;
using Android.Util;

namespace BlueWiFi
{
    class StationOperator
    {
        private readonly ContextWrapper m_ctx;
        private readonly BluetoothDevice m_device;
        private readonly StationOperatorCallback m_operationCallback;

        private BluetoothGatt m_gatt;
        public StationOperator(ContextWrapper ctx, BluetoothDevice dev)
        {
            m_ctx = ctx;
            m_device = dev;
            m_operationCallback = new StationOperatorCallback();

            OperationMaxWait = TimeSpan.FromMilliseconds(1000);
        }

        public TimeSpan OperationMaxWait { get; set; }

        public bool IsConnected {
            get {
                return m_operationCallback.IsConnected;
            }
        }

        public async Task<bool> Connect()
        {
            if (IsConnected) return true;

            m_operationCallback.BeginOperation();
            m_gatt = m_device.ConnectGatt(m_ctx, false, m_operationCallback);
            var taskWaitConnect = new Task<bool>(() => {
                return m_operationCallback.WaitOperationFinished(OperationMaxWait);
            });
            taskWaitConnect.Start();
            bool bOK =  await taskWaitConnect;
            if (!bOK) return false;

            m_operationCallback.BeginOperation();
            m_gatt.DiscoverServices();
            var taskWaitDiscover = new Task<bool>(() => {
                return m_operationCallback.WaitOperationFinished(OperationMaxWait);
            });
            taskWaitDiscover.Start();
            return await taskWaitDiscover;
        }

        public async Task<bool> Disconnect()
        {
            if (!IsConnected) return true;

            m_operationCallback.BeginOperation();
            m_gatt.Disconnect();
            var taskWait = new Task<bool>(() => {
                return m_operationCallback.WaitOperationFinished(OperationMaxWait);
            });
            taskWait.Start();
            await taskWait;

            return (!IsConnected);
        }

        public async Task<string> Read(Java.Util.UUID uuidService, Java.Util.UUID uuidCharacteristic)
        {
            var service = m_gatt.GetService(uuidService);
            if (service != null) {
                var cha = service.GetCharacteristic(uuidCharacteristic);
                if (cha != null) {
                    m_operationCallback.BeginOperation();
                    m_gatt.ReadCharacteristic(cha);
                    var taskWait = new Task<bool>(() => {
                        return m_operationCallback.WaitOperationFinished(OperationMaxWait);
                    });
                    taskWait.Start();
                    await taskWait;
                    string value = cha.GetStringValue(0);
                    return value.TrimEnd('\0'); ;
                }
            }

            return null;
        }

        public async Task<int> ReadU32(Java.Util.UUID uuidService, Java.Util.UUID uuidCharacteristic)
        {
            var service = m_gatt.GetService(uuidService);
            if (service != null) {
                var cha = service.GetCharacteristic(uuidCharacteristic);
                if (cha != null) {
                    m_operationCallback.BeginOperation();
                    m_gatt.ReadCharacteristic(cha);
                    var taskWait = new Task<bool>(() => {
                        return m_operationCallback.WaitOperationFinished(OperationMaxWait);
                    });
                    taskWait.Start();
                    await taskWait;
                    var value = cha.GetIntValue(GattFormat.Uint32, 0);
                    return value.IntValue();
                }
            }

            return 0;
        }

        public async Task<bool> Write(Java.Util.UUID uuidService, Java.Util.UUID uuidCharacteristic, string value)
        {
            var service = m_gatt.GetService(uuidService);
            if (service != null) {
                var cha = service.GetCharacteristic(uuidCharacteristic);
                if (cha != null) {
                    cha.SetValue($"{value}\0");
                    m_operationCallback.BeginOperation();
                    m_gatt.WriteCharacteristic(cha);
                    var taskWait = new Task<bool>(() => {
                        return m_operationCallback.WaitOperationFinished(OperationMaxWait);
                    });
                    taskWait.Start();
                    return await taskWait;
                }
            }

            return false;
        }
    }

    class StationOperatorCallback : BluetoothGattCallback
    {
        private readonly ManualResetEvent m_evFinished = new ManualResetEvent(true);

        public bool IsConnected { get; private set; }

        public void BeginOperation()
        {
            m_evFinished.Reset();
        }

        public void EndOperation()
        {
            m_evFinished.Set();
        }

        public bool WaitOperationFinished(TimeSpan tsWaitMax)
        {
            return m_evFinished.WaitOne(tsWaitMax);
        }

        public override void OnConnectionStateChange(BluetoothGatt gatt, [GeneratedEnum] GattStatus status, [GeneratedEnum] ProfileState newState)
        {
            base.OnConnectionStateChange(gatt, status, newState);

            if (newState == ProfileState.Connected) {
                IsConnected = true;
                EndOperation();
            }
            else if (newState == ProfileState.Disconnected) {
                Log.Info(Ref.TAG, "GATT disconnected");
                IsConnected = false;
                EndOperation();
            }
        }

        public override void OnServicesDiscovered(BluetoothGatt gatt, [GeneratedEnum] GattStatus status)
        {
            base.OnServicesDiscovered(gatt, status);
            if (status != GattStatus.Success) {
                Log.Warn(Ref.TAG, "GATT OnServicesDiscovered {0}", status);
            }
            EndOperation();
        }

        public override void OnCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, [GeneratedEnum] GattStatus status)
        {
            base.OnCharacteristicRead(gatt, characteristic, status);
            if (status == GattStatus.Success) {
                Log.Info(Ref.TAG, "GATT characteristic read");
                Log.Info(Ref.TAG, "\t uuid: {0}", characteristic.Uuid);
                Log.Info(Ref.TAG, "\t length: {0}", characteristic.GetStringValue(0).Length);
            }
            else {
                Log.Warn(Ref.TAG, "GATT OnCharacteristicRead {0}", status);
            }
            EndOperation();
        }

        public override void OnCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, [GeneratedEnum] GattStatus status)
        {
            base.OnCharacteristicWrite(gatt, characteristic, status);
            if (status == GattStatus.Success) {
                Log.Info(Ref.TAG, "GATT characteristic write");
            }
            else {
                Log.Warn(Ref.TAG, "GATT OnCharacteristicWrite {0}", status);
            }
            EndOperation();
        }
    }
}