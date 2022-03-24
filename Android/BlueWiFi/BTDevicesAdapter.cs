using System.Collections.Generic;
using Android.Bluetooth;
using Android.Content;
using Android.Views;
using Android.Widget;

namespace BlueWiFi
{
    internal class BTDevicesAdapter : BaseAdapter<BluetoothDevice>
    {
        private readonly Context m_ctx;
        private readonly IReadOnlyList<BluetoothDevice> m_devices;
        public BTDevicesAdapter(Context ctx, IReadOnlyList<BluetoothDevice> devices)
        {
            m_ctx = ctx;
            m_devices = devices;
        }

        public override int Count => m_devices.Count;

        public override View GetView(int position, View convertView, ViewGroup parent)
        {
            var inflater = LayoutInflater.From(m_ctx);
            var viewOuter = inflater.Inflate(Resource.Layout.bt_device, null);
            var viewTxt = viewOuter.FindViewById<TextView>(Resource.Id.dev_name);
            viewTxt.Text = m_devices[position].Name;
            return viewOuter;
        }

        public override BluetoothDevice this[int index] => m_devices[index];

        public override Java.Lang.Object GetItem(int position) => m_devices[position];

        public override long GetItemId(int position) => m_devices[position].GetHashCode();
    }
}