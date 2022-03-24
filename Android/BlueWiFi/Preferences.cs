using Android.App;
using Android.Content;
using Android.Content.Res;

namespace BlueWiFi
{
    class Preferences
    {
        private const string c_namePref = "BlueWiFi";
        public bool DevNameRegex { get; set; }
        public string DevName { get; set; }
        public int ScanMaxCount { get; set; }
        public int ScanMaxDuration { get; set; }

        public void Load(Activity activity)
        {
            var sharedPref = activity.GetSharedPreferences(c_namePref, FileCreationMode.Private);
            var Resources = activity.Resources;

            string key = Resources.GetResourceEntryName(Resource.Boolean.settings_default_dev_name_regex);
            bool bDevNameRegex = Resources.GetBoolean(Resource.Boolean.settings_default_dev_name_regex);
            DevNameRegex = sharedPref.GetBoolean(key, bDevNameRegex);

            key = Resources.GetResourceEntryName(Resource.String.settings_default_dev_name);
            string strDevName = Resources.GetString(Resource.String.settings_default_dev_name);
            DevName = sharedPref.GetString(key, strDevName);

            key = Resources.GetResourceEntryName(Resource.Integer.settings_default_scan_max_count);
            int nScanMaxCount = Resources.GetInteger(Resource.Integer.settings_default_scan_max_count);
            ScanMaxCount = sharedPref.GetInt(key, nScanMaxCount);

            key = Resources.GetResourceEntryName(Resource.Integer.settings_default_scan_max_duration);
            int nScanMaxDuration = Resources.GetInteger(Resource.Integer.settings_default_scan_max_duration);
            ScanMaxDuration = sharedPref.GetInt(key, nScanMaxDuration);
        }

        public void Save(Activity activity)
        {
            var sharedPref = activity.GetSharedPreferences(c_namePref, FileCreationMode.Private);
            var Resources = activity.Resources;
            var editor = sharedPref.Edit();

            string key = Resources.GetResourceEntryName(Resource.Boolean.settings_default_dev_name_regex);
            editor.PutBoolean(key, DevNameRegex);

            key = Resources.GetResourceEntryName(Resource.String.settings_default_dev_name);
            editor.PutString(key, DevName);

            key = Resources.GetResourceEntryName(Resource.Integer.settings_default_scan_max_count);
            editor.PutInt(key, ScanMaxCount);

            key = Resources.GetResourceEntryName(Resource.Integer.settings_default_scan_max_duration);
            editor.PutInt(key, ScanMaxDuration);

            editor.Apply();
        }

        public override string ToString()
        {
            return $"scan for {ScanMaxCount} devices in {ScanMaxDuration}ms, match {DevName} rgx:{DevNameRegex}";
        }
    }
}