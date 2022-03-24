
using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using Android.App;
using Android.Content;
using Android.OS;
using Android.Util;
using Android.Views;
using Android.Widget;
using AndroidX.AppCompat.App;
using Google.Android.Material.FloatingActionButton;
using Google.Android.Material.Snackbar;

namespace BlueWiFi
{
    [Activity(Label = "DevSetupActivity", ParentActivity = typeof(MainActivity))]
    public class DevSetupActivity : AppCompatActivity, IServiceConnection
    {

        private readonly Java.Util.UUID c_uuidSrv = Java.Util.UUID.FromString("54bc86d0-6e0c-47ac-9200-7a3e16c2a980");
        private readonly Java.Util.UUID c_uuidCha = Java.Util.UUID.FromString("54bc86d0-6e0c-47ac-9300-7a3e16c2a981");
        private readonly Java.Util.UUID c_uuidChaIP = Java.Util.UUID.FromString("54bc86d0-6e0c-47ac-9300-7a3e16c2a982");

        private bool m_bInitialized = false;
        private StationBLEBinder m_binderStationBLE;

        private string m_strDevName;
        private string m_strDevAddr;

        private StationOperator m_gattOp;

        TextView m_txtViewGattValue;

        private readonly List<WiFiEntry> m_listEntries = new List<WiFiEntry>();
        private ListView m_listWiFi;
        private WiFiEntryAdapter m_adapterWiFi;

        protected override void OnCreate(Bundle savedInstanceState)
        {
            base.OnCreate(savedInstanceState);
            SetContentView(Resource.Layout.activity_dev_setup);

            SupportActionBar.SetDisplayHomeAsUpEnabled(true);

            m_strDevName = Intent.GetStringExtra("Name");
            m_strDevAddr = Intent.GetStringExtra("Address");

            this.Title = m_strDevName;
            TextView txtView = FindViewById<TextView>(Resource.Id.dev_name);
            txtView.Text = $"MAC {m_strDevAddr}";

            m_txtViewGattValue = FindViewById<TextView>(Resource.Id.gatt_value);

            var btnAdd = FindViewById<FloatingActionButton>(Resource.Id.add);
            btnAdd.Click += OnAddEntry;

            var btnSave = FindViewById<FloatingActionButton>(Resource.Id.save);
            btnSave.Click += OnSave;

            m_listWiFi = FindViewById<ListView>(Resource.Id.listWiFi);
            m_adapterWiFi = new WiFiEntryAdapter(this, m_listEntries);
            m_listWiFi.Adapter = m_adapterWiFi;

            var intent = new Intent(this, typeof(StationBLEService));
            BindService(intent, this, Bind.AutoCreate);
        }

        protected override void OnResume()
        {
            base.OnResume();

        }

        protected override void OnPause()
        {
            base.OnPause();
            if (m_gattOp != null && m_gattOp.IsConnected) {
                m_gattOp.Disconnect().Wait(0);
            }
        }

        protected override void OnDestroy()
        {
            base.OnDestroy();
        }

        private void Initialize()
        {
            m_bInitialized = true;
            // Read GATT data
            GattRead();
        }

        private void Deinitialize()
        {
            m_bInitialized = false;
        }

        public void OnServiceConnected(ComponentName name, IBinder service)
        {
            m_binderStationBLE = (StationBLEBinder)service;
            Initialize();
        }

        public void OnServiceDisconnected(ComponentName name)
        {
            Log.Error(Ref.TAG, "StationBLEService disconnected!");
            Deinitialize();
        }

        public override void OnCreateContextMenu(IContextMenu menu, View v, IContextMenuContextMenuInfo menuInfo)
        {
            base.OnCreateContextMenu(menu, v, menuInfo);
            MenuInflater.Inflate(Resource.Menu.menu_wifi, menu);
            var mi = menu.GetItem(0);
            mi.SetActionView(v);

        }

        public override bool OnContextItemSelected(IMenuItem item)
        {
            if (item.ItemId == Resource.Id.delete) {
                WiFiEntry entry = (WiFiEntry)item.ActionView.Tag;
                m_listEntries.Remove(entry);
                m_adapterWiFi.NotifyDataSetChanged();
            }
            return base.OnContextItemSelected(item);
        }

        private async void GattRead()
        {
            if (!m_bInitialized) return;

            m_gattOp = m_binderStationBLE.StationBLESrv.CreateGattOperation(m_strDevName);
            if (await m_gattOp.Connect()) {
                string bufGatt = await m_gattOp.Read(c_uuidSrv, c_uuidCha);
                LoadEntries(bufGatt);
                int nIPv4 = await m_gattOp.ReadU32(c_uuidSrv, c_uuidChaIP);
                byte[] b = BitConverter.GetBytes(nIPv4);
                m_txtViewGattValue.Text = $"{b[0]}.{b[1]}.{b[2]}.{b[3]}";
            }
            else {
                m_txtViewGattValue.Text = "GATT connect failed!";
                Log.Error(Ref.TAG, m_txtViewGattValue.Text);
            }
        }

        private void LoadEntries(string buf)
        {
            if(String.IsNullOrEmpty(buf)) return;

            m_listEntries.Clear();
            var lines = buf.Split('\n');
            WiFiEntry entry = null;
            for (int i = 0; i < lines.Length; i++) {
                if (i % 2 == 0) {
                    entry = new WiFiEntry();
                    entry.SSID = lines[i];
                }
                else {
                    entry.Pwd = lines[i];
                    m_listEntries.Add(entry);
                }
            }

            m_adapterWiFi.NotifyDataSetChanged();

        }

        private void OnAddEntry(object sender, EventArgs e)
        {
            // max 3 entries
            if(m_listEntries.Count >= 3) return;

            m_listEntries.Add(new WiFiEntry());
            m_adapterWiFi.NotifyDataSetChanged();
        }

        private async void OnSave(object sender, EventArgs e)
        {
            StringBuilder builder = new StringBuilder();
            foreach (WiFiEntry entry in m_listEntries) {
                if (String.IsNullOrEmpty(entry.SSID) || String.IsNullOrEmpty(entry.Pwd)) {
                    string bufx = "The SSID and Password should not be empty!";
                    Snackbar.Make((View)sender, bufx, Snackbar.LengthLong).Show();
                    return;

                }
                builder.AppendFormat("{0}\n{1}\n", entry.SSID, entry.Pwd);
            }

            bool bOkay = false;
            if (!m_gattOp.IsConnected) {
                // reconnect if lost
                bOkay = await m_gattOp.Connect();
                if (!bOkay) {
                    Snackbar.Make((View)sender, "GATT disconnected", Snackbar.LengthLong).Show();
                    return;
                }
            }

            bOkay = await m_gattOp.Write(c_uuidSrv, c_uuidCha, builder.ToString());
            string buf = bOkay ? "GATT success!" : "GATT failed";
            Snackbar.Make((View)sender, buf, Snackbar.LengthLong).Show();

        }
    }
}