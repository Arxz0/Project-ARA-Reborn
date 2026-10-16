using System;
using System.Windows;

namespace RebornSetup
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            DispatcherUnhandledException += (s, a) =>
            {
                Engine.Log("! unhandled: " + a.Exception);
                MessageBox.Show(a.Exception.Message + "\n\n" + Engine.LogPath, "Project ARA: Reborn", MessageBoxButton.OK, MessageBoxImage.Error);
                a.Handled = true;
            };
            base.OnStartup(e);
        }
    }
}
