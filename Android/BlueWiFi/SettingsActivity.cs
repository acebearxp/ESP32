using System;

using Android.App;
using Android.Content;
using Android.OS;
using Android.Widget;
using AndroidX.AppCompat.App;

namespace BlueWiFi
{
    [Activity(ParentActivity = typeof(MainActivity))]
    class SettingsActivity: AppCompatActivity
    {
        private CheckBox m_checkDevNameRegex;
        private EditText m_editDevName;
        private EditText m_editScanMaxCount;
        private EditText m_editScanMaxDuration;

        private readonly Preferences m_pref = new Preferences();

        protected override void OnCreate(Bundle savedInstanceState)
        {
            base.OnCreate(savedInstanceState);
            SetContentView(Resource.Layout.activity_settings);
            SupportActionBar.SetDisplayHomeAsUpEnabled(true);

            m_checkDevNameRegex = FindViewById<CheckBox>(Resource.Id.settings_dev_name_regex);
            m_editDevName = FindViewById<EditText>(Resource.Id.settings_dev_name);
            m_editScanMaxCount = FindViewById<EditText>(Resource.Id.settings_scan_max_count);
            m_editScanMaxDuration = FindViewById<EditText>(Resource.Id.settings_scan_max_duration);

            m_pref.Load(this);

            m_checkDevNameRegex.Checked = m_pref.DevNameRegex;
            m_editDevName.Text = m_pref.DevName;
            m_editScanMaxCount.Text = $"{m_pref.ScanMaxCount}";
            m_editScanMaxDuration.Text = $"{m_pref.ScanMaxDuration}";
        }

        protected override void OnPause()
        {
            m_pref.DevNameRegex = m_checkDevNameRegex.Checked;
            m_pref.DevName = m_editDevName.Text;
            m_pref.ScanMaxCount = Int32.Parse(m_editScanMaxCount.Text);
            m_pref.ScanMaxDuration = Int32.Parse(m_editScanMaxDuration.Text);

            m_pref.Save(this);

            base.OnPause();
        }
    }
}