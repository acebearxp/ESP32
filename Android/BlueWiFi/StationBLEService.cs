using System.Threading;
using System.Threading.Tasks;
using Android.App;
using Android.Content;
using Android.OS;
using Android.Util;

namespace BlueWiFi
{
    /// <summary>
    /// 低功耗蓝牙
    /// </summary>
    [Service(Name ="com.acebear.bluwifi.StationBLEService")]
    public class StationBLEService : Service
    {
        private StationBLEBinder m_binder;
        public override void OnCreate()
        {
            base.OnCreate();
        }
        public override IBinder OnBind(Intent intent)
        {
            m_binder = new StationBLEBinder(new StationBLE(this));
            m_binder.StationBLESrv.Init();
            return this.m_binder;
        }

        public override bool OnUnbind(Intent intent)
        {
            m_binder.StationBLESrv.Uninit();
            return base.OnUnbind(intent);
        }

        public override void OnDestroy()
        {
            base.OnDestroy();
        }
    }

    class StationBLEBinder : Binder
    {
        public StationBLEBinder(StationBLE station) {
            this.StationBLESrv = station;
        }

        public StationBLE StationBLESrv { get; private set; }
    }
}