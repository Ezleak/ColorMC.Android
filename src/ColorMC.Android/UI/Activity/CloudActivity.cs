﻿using Android.App;
using Android.Content;
using Android.OS;
using Android.Widget;
using AndroidX.AppCompat.App;
using ColorMC.Gui.Utils;

namespace ColorMC.Android.UI.Activity;

[Activity(Name = "colormc.android.CloudActivity", Exported = true)]
[IntentFilter([Intent.ActionView], Categories =
[
    "android.intent.category.BROWSABLE", "android.intent.category.DEFAULT"
], DataScheme = "colormc", DataHost = "colormc", DataPath = "/cloud", DataPort = "80")]
public class CloudActivity : AppCompatActivity
{
    protected override void OnCreate(Bundle? savedInstanceState)
    {
        base.OnCreate(savedInstanceState);

        if (Intent?.DataString is { } str)
        {
            var args = str.Split('?');
            if (args.Length > 1)
            {
                var key = args[1];
                GuiConfigUtils.Init(GetExternalFilesDir(null).AbsolutePath + "/");
                GuiConfigUtils.Config.ServerKey = key;
                GuiConfigUtils.SaveNow();

                Toast.MakeText(ApplicationContext, "已导入云服务器密钥", ToastLength.Short)?.Show();

                Finish();
            }
        }
    }
}
