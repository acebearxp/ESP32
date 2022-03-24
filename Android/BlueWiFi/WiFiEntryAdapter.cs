using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Android.App;
using Android.Content;
using Android.OS;
using Android.Runtime;
using Android.Views;
using Android.Widget;

namespace BlueWiFi
{
    class WiFiEntryAdapter : BaseAdapter
    {
        private readonly ContextWrapper m_ctx;
        private readonly IReadOnlyList<WiFiEntry> m_listEntries;
        public WiFiEntryAdapter(ContextWrapper ctx, IReadOnlyList<WiFiEntry> listEntries)
        {
            m_ctx = ctx;
            m_listEntries = listEntries;
        }

        public override int Count => m_listEntries.Count;

        public override Java.Lang.Object GetItem(int position)
        {
            return m_listEntries[position];
        }

        public override long GetItemId(int position)
        {
            return m_listEntries[position].GetHashCode();
        }

        public override View GetView(int position, View convertView, ViewGroup parent)
        {
            var target = m_listEntries[position];

            var inflater = LayoutInflater.From(m_ctx);
            var viewOuter = inflater.Inflate(Resource.Layout.ssid_pwd, null);

            Activity actOuter = (Activity)m_ctx;
            var btn = viewOuter.FindViewById(Resource.Id.btnMenu);
            btn.Tag = target;
            actOuter.RegisterForContextMenu(btn);

            var editorS = viewOuter.FindViewById<EditText>(Resource.Id.editSSID);
            editorS.Text = m_listEntries[position].SSID;
            editorS.TextChanged += (sender, e) => { target.SSID = editorS.Text;  };

            var editorP = viewOuter.FindViewById<EditText>(Resource.Id.editPWD);
            editorP.Text = m_listEntries[position].Pwd;
            editorP.TextChanged += (sender, e) => { target.Pwd = editorP.Text; };
                        
            return viewOuter;
        }
    }
}