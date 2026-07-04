using System;
using System.Diagnostics;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using Microsoft.Win32;

namespace PebbleInjector
{
    public partial class MainWindow : Window
    {
        private bool _isBusy;
        private ConsoleWindow _console;

        public MainWindow()
        {
            InitializeComponent();
            var version = Assembly.GetExecutingAssembly().GetName().Version;
            VersionLabel.Content = $"v{version.Major}.{version.Minor}";
        }

        private void SetStatus(string status)
        {
            Dispatcher.Invoke(() =>
            {
                InjectButton.Content = status == "done" ? "inject" : $"{status}...";
                _console?.Append(status);
            });
        }

        private void FinishOperation()
        {
            _isBusy = false;
            InjectButton.Content = "inject";
        }

        private async void InjectButton_Click(object sender, MouseButtonEventArgs e)
        {
            if (_isBusy) return;

            var dialog = new OpenFileDialog
            {
                Filter = "DLL files (*.dll)|*.dll",
                InitialDirectory = AppDomain.CurrentDomain.BaseDirectory,
                FileName = "PebbleCore.dll",
                RestoreDirectory = true,
                CheckFileExists = true,
                Multiselect = false,
                Title = "Choose a DLL to inject"
            };
            if (dialog.ShowDialog() != true) return;

            _isBusy = true;
            try
            {
                var process = FindMinecraftProcess();
                if (process == null)
                {
                    SetStatus("launching Minecraft");
                    Process.Start(new ProcessStartInfo
                    {
                        FileName = "explorer.exe",
                        Arguments = "shell:appsFolder\\Microsoft.MinecraftUWP_8wekyb3d8bbwe!App",
                        UseShellExecute = true
                    });
                    process = await WaitForMinecraftAsync(TimeSpan.FromSeconds(30));
                }

                if (process == null)
                {
                    throw new TimeoutException("Minecraft did not start within 30 seconds.");
                }

                await InjectAsync(dialog.FileName, process);
                MessageBox.Show(
                    "DLL injected successfully.",
                    "PebbleInjector",
                    MessageBoxButton.OK,
                    MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                ShowError(ex);
            }
            finally
            {
                FinishOperation();
            }
        }

        private static Process FindMinecraftProcess()
        {
            foreach (var process in Process.GetProcessesByName("Minecraft.Windows"))
            {
                try
                {
                    if (!process.HasExited) return process;
                }
                catch
                {
                    // The process disappeared while it was being inspected.
                }

                process.Dispose();
            }

            return null;
        }

        private static async Task<Process> WaitForMinecraftAsync(TimeSpan timeout)
        {
            var deadline = DateTime.UtcNow + timeout;
            while (DateTime.UtcNow < deadline)
            {
                var process = FindMinecraftProcess();
                if (process != null) return process;
                await Task.Delay(250);
            }

            return null;
        }

        private Task InjectAsync(string dllPath, Process process)
        {
            var processId = checked((uint)process.Id);
            process.Dispose();
            return Task.Run(() => InjectorBase.Inject(dllPath, processId, SetStatus));
        }

        private void ShowError(Exception exception)
        {
            MessageBox.Show(
                exception.Message,
                "Injection failed",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
            _console?.Append($"Error: {exception}");
        }

        private void CloseWindow(object sender, MouseButtonEventArgs e)
        {
            Application.Current.Shutdown();
        }

        private void DragWindow(object sender, MouseButtonEventArgs e)
        {
            if (e.LeftButton == MouseButtonState.Pressed)
            {
                DragMove();
            }
        }

        private void ConsoleButton_Click(object sender, RoutedEventArgs e)
        {
            if (_console == null)
            {
                _console = CreateConsoleWindow();
            }

            if (_console.IsVisible)
            {
                _console.Hide();
            }
            else
            {
                _console.Show();
                _console.Activate();
            }
        }

        private ConsoleWindow CreateConsoleWindow()
        {
            var window = new ConsoleWindow { Owner = this };
            window.Closed += (sender, args) => _console = null;
            return window;
        }
    }

}
