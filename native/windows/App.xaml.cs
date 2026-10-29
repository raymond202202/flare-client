using System;
using System.IO;
using System.Text;
using System.Windows;
using System.Windows.Threading;

namespace FlareWin
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            // 全局兜底：任何未捕获异常都写日志并提示，绝不静默消失
            DispatcherUnhandledException += OnUnhandled;
            AppDomain.CurrentDomain.UnhandledException += (s, args) =>
            {
                Log("APPDOMAIN", args.ExceptionObject as Exception);
            };

            var w = new MainWindow();
            MainWindow = w;
            w.Show();

            // 自测钩子：FLARE_GUI_AUTOCLOSE=N 秒后自动退出（CI 冒烟用）
            var ac = Environment.GetEnvironmentVariable("FLARE_GUI_AUTOCLOSE");
            if (int.TryParse(ac, out var secs) && secs > 0)
            {
                var t = new DispatcherTimer { Interval = TimeSpan.FromSeconds(secs) };
                t.Tick += (s2, e2) =>
                {
                    t.Stop();
                    w.Close();
                };
                t.Start();
            }
        }

        private void OnUnhandled(object sender, DispatcherUnhandledExceptionEventArgs e)
        {
            Log("DISPATCHER", e.Exception);
            MessageBox.Show(
                "Flare 遇到内部错误，已记录：\n" + e.Exception?.Message,
                "Flare", MessageBoxButton.OK, MessageBoxImage.Warning);
            // 标记已处理，避免整个应用被一句异常带走
            e.Handled = true;
        }

        private static void Log(string tag, Exception ex)
        {
            var p = Environment.GetEnvironmentVariable("FLARE_GUI_LOG");
            if (string.IsNullOrEmpty(p)) return;
            try
            {
                File.AppendAllText(p,
                    DateTime.Now.ToString("HH:mm:ss.fff") + " !!! " + tag + " " + ex + "\n",
                    Encoding.UTF8);
            }
            catch
            {
            }
        }
    }
}
