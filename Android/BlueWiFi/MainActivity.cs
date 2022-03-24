using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using Android;
using Android.App;
using Android.Content;
using Android.Content.PM;
using Android.Graphics;
using Android.OS;
using Android.Runtime;
using Android.Util;
using Android.Views;
using Android.Widget;
using AndroidX.AppCompat.App;
using Google.Android.Material.FloatingActionButton;
using Google.Android.Material.Snackbar;
using Toolbar = AndroidX.AppCompat.Widget.Toolbar;

namespace BlueWiFi
{
    [Activity(Label = "@string/app_name", Theme = "@style/AppTheme.NoActionBar", MainLauncher = true, LaunchMode = LaunchMode.SingleTop)]
    public class MainActivity : AppCompatActivity, IServiceConnection
    {
        private Toolbar m_toolbar;
        private FloatingActionButton m_btnBTScan;

        private List<Android.Bluetooth.BluetoothDevice> m_listScannedDevices;
        private BTDevicesAdapter m_adapterDevices;

        private bool m_bInitialized = false;
        private StationBLEBinder m_binderStationBLE;

        private readonly Preferences m_pref = new Preferences();

        protected override void OnCreate(Bundle savedInstanceState)
        {
            base.OnCreate(savedInstanceState);

            Xamarin.Essentials.Platform.Init(this, savedInstanceState);
            SetContentView(Resource.Layout.activity_main);

            LogDeviceInfo();
            AskForPermissions();

            m_toolbar = FindViewById<Toolbar>(Resource.Id.toolbar);
            SetSupportActionBar(m_toolbar);

            m_listScannedDevices = new List<Android.Bluetooth.BluetoothDevice>();
            m_btnBTScan = FindViewById<FloatingActionButton>(Resource.Id.fab);
            m_btnBTScan.Click += OnScan;

            ListView lvDevices = FindViewById<ListView>(Resource.Id.listBTDevices);
            m_adapterDevices = new BTDevicesAdapter(this, m_listScannedDevices);
            lvDevices.Adapter = m_adapterDevices;
            lvDevices.ItemClick += OnDeviceClick;

            var intent = new Intent(this, typeof(StationBLEService));
            BindService(intent, this, Bind.AutoCreate);
        }

        protected override void OnResume()
        {
            base.OnResume();
            m_pref.Load(this);
        }

        protected override void OnPause()
        {
            base.OnPause();
        }

        protected override void OnDestroy()
        {
            Deinitialize();
            UnbindService(this);
            base.OnDestroy();
        }

        public override bool OnCreateOptionsMenu(IMenu menu)
        {
            MenuInflater.Inflate(Resource.Menu.menu_main, menu);
            return true;
        }

        public override bool OnOptionsItemSelected(IMenuItem item)
        {
            int id = item.ItemId;
            if (id == Resource.Id.action_settings)
            {
                Intent itSettings = new Intent(this, typeof(SettingsActivity));
                StartActivity(itSettings); 
                return true;
            }

            return base.OnOptionsItemSelected(item);
        }

        private void LogDeviceInfo()
        {
            Log.Info(Ref.TAG, "Device: {0} {1} {2}",
                // m_scStationBLE.Service.StationName,
                Xamarin.Essentials.DeviceInfo.Manufacturer,
                Xamarin.Essentials.DeviceInfo.Platform,
                Xamarin.Essentials.DeviceInfo.VersionString);
            Log.Info(Ref.TAG, "Chip:   {0}", Build.Hardware);
            Log.Info(Ref.TAG, "Build:  {0}", Build.Display);
        }

        private bool AskForPermissions()
        {
            var listPermissions = new List<String>();
            string[] permissions = {
                Manifest.Permission.AccessCoarseLocation,
                Manifest.Permission.AccessFineLocation,
                Manifest.Permission.Bluetooth,
                Build.VERSION.SdkInt < BuildVersionCodes.S ?
                    Manifest.Permission.BluetoothAdmin:Manifest.Permission.BluetoothScan,
                Build.VERSION.SdkInt < BuildVersionCodes.S ?
                    Manifest.Permission.BluetoothAdmin:Manifest.Permission.BluetoothConnect
            };

            foreach (var px in permissions) {
                if (CheckSelfPermission(px) == Permission.Denied){
                    listPermissions.Add(px);

                    if (ShouldShowRequestPermissionRationale(px))
                        Log.Warn(Ref.TAG, "Need show reason for {0}", px);
                    else
                        Log.Info(Ref.TAG, "No need show reason for {0}", px);
                }
                else Log.Info(Ref.TAG, "Has granted {0}", px);
            }
            
            if (listPermissions.Count > 0) {
                RequestPermissions(listPermissions.Distinct().ToArray(), 1);
            }

            return false;
        }

        public override void OnRequestPermissionsResult(int requestCode, string[] permissions, [GeneratedEnum] Permission[] grantResults)
        {
            Xamarin.Essentials.Platform.OnRequestPermissionsResult(requestCode, permissions, grantResults);
            base.OnRequestPermissionsResult(requestCode, permissions, grantResults);

            for (int i = 0; i < permissions.Length && i < grantResults.Length; i++) {
                Log.Info(Ref.TAG, "Permission : {0} : {1}",
                    permissions[i], grantResults[i]);
            }
        }

        private void Initialize()
        {
            OnBTStateChanged(m_binderStationBLE.StationBLESrv.IsBTEnabled);
            m_binderStationBLE.StationBLESrv.OnBTEnableChanged += OnBTStateChanged;
            m_bInitialized = true;
        }

        private void Deinitialize()
        {
            m_bInitialized = false;
        }

        private void OnBTStateChanged(bool bEnabled)
        {
            if (bEnabled){
                m_btnBTScan.Clickable = true;
                m_btnBTScan.Background.SetTint(ApplicationContext.GetColor(Resource.Color.colorAccent));
            }
            else{
                m_btnBTScan.Clickable = false;
                m_btnBTScan.Background.SetTint(Color.DarkGray);
            }
        }

        private async void OnScan(object sender, EventArgs e)
        {
            // clear
            m_listScannedDevices.Clear();
            m_adapterDevices.NotifyDataSetChanged();
            m_btnBTScan.Clickable = false;
            m_btnBTScan.Background.SetTint(ApplicationContext.GetColor(Resource.Color.colorPrimary));

            if (m_bInitialized) {
                var listResult = await m_binderStationBLE.StationBLESrv.ScanAsync(
                    TimeSpan.FromMilliseconds(m_pref.ScanMaxDuration), m_pref.ScanMaxCount, m_pref.DevName, m_pref.DevNameRegex);

                foreach (var x in listResult) {
                    Log.Info(Ref.TAG, "BLE Scan found: {0} {1}", x.Name, x.Address);
                }
                m_listScannedDevices.AddRange(listResult);
                m_adapterDevices.NotifyDataSetChanged();
            }
            else {
                Snackbar.Make((View)sender, "Service is not readly!", Snackbar.LengthLong).Show();
            }

            m_btnBTScan.Background.SetTint(ApplicationContext.GetColor(Resource.Color.colorAccent));
            m_btnBTScan.Clickable = true;
        }

        private void OnDeviceClick(object sender, AdapterView.ItemClickEventArgs e)
        {
            var device = m_listScannedDevices[e.Position];
            Intent itDevSetup = new Intent(this, typeof(DevSetupActivity));
            itDevSetup.PutExtra("Name", device.Name);
            itDevSetup.PutExtra("Address", device.Address);
            StartActivity(itDevSetup);
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
    }
}
