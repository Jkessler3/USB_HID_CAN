using System.IO;
using System.Windows;
using System.Windows.Threading;

namespace CANaerospaceJoystickMapper.App;

public partial class App : Application
{
    private static readonly string LogDirectory =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                     "CANaerospace Joystick Mapper");

    private static readonly string LogPath = Path.Combine(LogDirectory, "startup.log");

    protected override void OnStartup(StartupEventArgs e)
    {
        AppDomain.CurrentDomain.UnhandledException += (_, args) =>
            WriteLog("Unhandled exception", args.ExceptionObject as Exception);
        DispatcherUnhandledException += OnDispatcherUnhandledException;
        TaskScheduler.UnobservedTaskException += (_, args) =>
        {
            WriteLog("Unobserved task exception", args.Exception);
            args.SetObserved();
        };

        try
        {
            WriteLog("Starting CANaerospace Joystick Mapper.");
            base.OnStartup(e);

            var window = new MainWindow();
            MainWindow = window;
            window.Show();
            WriteLog("Main window shown.");
        }
        catch (Exception ex)
        {
            WriteLog("Startup failed", ex);
            MessageBox.Show($"CANaerospace Joystick Mapper failed to start.\n\n{ex.Message}\n\nLog: {LogPath}",
                            "Startup Error",
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
            Shutdown(1);
        }
    }

    private static void OnDispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        WriteLog("Dispatcher exception", e.Exception);
        MessageBox.Show($"CANaerospace Joystick Mapper hit an error.\n\n{e.Exception.Message}\n\nLog: {LogPath}",
                        "Application Error",
                        MessageBoxButton.OK,
                        MessageBoxImage.Error);
        e.Handled = true;
    }

    private static void WriteLog(string message, Exception? exception = null)
    {
        try
        {
            Directory.CreateDirectory(LogDirectory);
            File.AppendAllText(LogPath,
                $"[{DateTimeOffset.Now:O}] {message}{Environment.NewLine}{exception}{Environment.NewLine}");
        }
        catch
        {
            // Last-resort diagnostics should never prevent startup.
        }
    }
}
